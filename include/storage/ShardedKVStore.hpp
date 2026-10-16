#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <vector>
#include <functional>
#include <list>
#include <chrono>
#include "../utils/SlabAllocator.hpp"

using LRUList = std::list<std::string, SlabAllocator<std::string>>;

struct CacheItem{
    std::string value;
    LRUList::iterator lru_it;
    std::chrono::steady_clock::time_point expiry;
};

using CacheMap = std::unordered_map<
    std::string, 
    CacheItem, 
    std::hash<std::string>, 
    std::equal_to<std::string>, 
    SlabAllocator<std::pair<const std::string, CacheItem>>
>;

// Individual bucket structure encapsulating a map and its specific lock
struct LRUBucket {
    // The list tracks usage order: most recent at the front, oldest at the back
    LRUList lru_list;

    // The map stores the value AND an iterator pointing directly to the key's position in the list
    CacheMap store;
    
    mutable std::shared_mutex rw_mutex;
    size_t max_capacity;    // Max capacity of the bucket
};

class ShardedKVStore {
    private:
        std::vector<LRUBucket> buckets;
        size_t num_shards;

        // Routing a key to a bucket
        size_t get_bucket_index(const std::string& key) const
        {
            return std::hash<std::string>{}(key) % num_shards;
        }
        // std::hash<std::string> is a template functor, whose default ctor we call(note the {}), and then call the functor's operator(), by passing in our 'key' as a param
    
    public:
        explicit ShardedKVStore(size_t shards = 32,size_t capacity_per_shard = 1000): num_shards(shards),buckets(shards)
        {
            for(auto& bucket: buckets)
            {
                bucket.max_capacity = capacity_per_shard;
            }
        }

        // Exclusive access to lock
        void set(const std::string& key,const std::string& value,int ttl_seconds = -1)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);

            auto expiry_time = (ttl_seconds > 0) ? 
                std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds) : 
                std::chrono::steady_clock::time_point::max();
            
            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                // Key exists: Update value and move it to the front of the LRU list
                it -> second.value = value;
                it -> second.expiry = expiry_time;
                buckets[idx].lru_list.erase(it -> second.lru_it);
                buckets[idx].lru_list.push_front(key);
                it -> second.lru_it = buckets[idx].lru_list.begin();
            }
            // Not really optimal. erase() + push_front() deallocates, then allocates again, which creates two round-trips to the memory-pool, and each round-trip involves locking and unlocking the mutex (mtx)
            else
            {
                // New key: Check if we are at capacity first
                if(buckets[idx].store.size() >= buckets[idx].max_capacity)
                {
                    // Evict the least recently used item (the one at the back of the list)
                    std::string oldest_key = buckets[idx].lru_list.back();
                    buckets[idx].lru_list.pop_back();
                    buckets[idx].store.erase(oldest_key);
                }

                // Insert new key at the front
                buckets[idx].lru_list.push_front(key);
                buckets[idx].store[key] = {value,buckets[idx].lru_list.begin(),expiry_time};

                // Again not really optimal: first the cache-item for the new key is default-constructed, and then copy assigned to our RHS. A better way would be to use try_emplace
            }
        }

        // Exclusive access to lock (when we implement LRU Cache)
        std::optional <std::string> get(const std::string& key)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);

            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                if (std::chrono::steady_clock::now() > it->second.expiry) {
                    buckets[idx].lru_list.erase(it->second.lru_it);
                    buckets[idx].store.erase(it);
                    return std::nullopt;
                }

                // Move the accessed key to the front (marking it as recently used)
                buckets[idx].lru_list.erase(it -> second.lru_it);
                buckets[idx].lru_list.push_front(key);
                it -> second.lru_it = buckets[idx].lru_list.begin();

                return it -> second.value;
            }
            return std::nullopt;
        }

        // Exclusive access to lock
        bool remove(const std::string& key)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);
            
            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                // Remove from both the list and the map
                buckets[idx].lru_list.erase(it -> second.lru_it);
                buckets[idx].store.erase(it);
                return true;
            }
            return false;
        }

        // Background task to actively sweep and delete all expired keys across all shards
        void clean_expired_keys()
        {
            auto now = std::chrono::steady_clock::now();
            for(size_t i = 0; i < num_shards; i++) 
            {
                std::unique_lock<std::shared_mutex> lock(buckets[i].rw_mutex);
                for(auto it = buckets[i].store.begin(); it != buckets[i].store.end(); ) 
                {
                    if(now > it->second.expiry) 
                    {
                        buckets[i].lru_list.erase(it->second.lru_it);
                        it = buckets[i].store.erase(it);
                    } 
                    else 
                    {
                        ++it;
                    }
                }
            }
        }
};

/*
The standard container API requires you to write SlabAllocator<T> where T is the element type, but the container never allocates bare T instances. It immediately rebinds the allocator to its internal node structs (_List_node and _Hash_node), so your SlabAllocator only ever allocates and frees complete nodes containing the pointers, hashes, and payload data packed together.
*/

/*
Possible Improvements:

1. Per-shard SlabManager: Eliminates global allocator lock contention across concurrent worker threads.
2. store.reserve(max_capacity): Pre-sizes the hash table to prevent rehashing and OS heap fallbacks.
3. store.try_emplace(): Constructs CacheItem directly in-place, eliminating redundant temporary object creation.
*/
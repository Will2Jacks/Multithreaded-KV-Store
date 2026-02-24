#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <vector>
#include <functional>
#include <list>

// Individual bucket structure encapsulating a map and its specific lock
struct LRUBucket {
    // The list tracks usage order: most recent at the front, oldest at the back
    std::list <std::string> lru_list;

    // The map stores the value AND an iterator pointing directly to the key's position in the list
    std::unordered_map <std::string,std::pair <std::string,std::list <std::string>::iterator>> store;
    
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
    
    public:
        explicit ShardedKVStore(size_t shards = 32,size_t capacity_per_shard = 1000): num_shards(shards),buckets(shards)
        {
            for(auto& bucket: buckets)
            {
                bucket.max_capacity = capacity_per_shard;
            }
        }

        // Exclusive access to lock
        void set(const std::string& key,const std::string& value)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);
            
            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                // Key exists: Update value and move it to the front of the LRU list
                it -> second.first = value;
                buckets[idx].lru_list.erase(it -> second.second);
                buckets[idx].lru_list.push_front(key);
                it -> second.second = buckets[idx].lru_list.begin();
            }
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
                buckets[idx].store[key] = {value,buckets[idx].lru_list.begin()};
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
                // Move the accessed key to the front (marking it as recently used)
                buckets[idx].lru_list.erase(it -> second.second);
                buckets[idx].lru_list.push_front(key);
                it -> second.second = buckets[idx].lru_list.begin();

                return it -> second.first;
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
                buckets[idx].lru_list.erase(it -> second.second);
                buckets[idx].store.erase(it);
                return true;
            }
            return false;
        }
};
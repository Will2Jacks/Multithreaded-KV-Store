#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <vector>
#include <functional>

// Individual bucket structure encapsulating a map and its specific lock
struct KVPairBucket {
    std::unordered_map <std::string,std::string> store;
    mutable std::shared_mutex rw_mutex;      // so that const methods can lock
};

class ShardedKVStore {
    private:
        std::vector<KVPairBucket> buckets;
        size_t num_shards;

        // Routing a key to a bucket
        size_t get_bucket_index(const std::string& key) const
        {
            return std::hash<std::string>{}(key) % num_shards;
        }
    
    public:
        explicit ShardedKVStore(size_t shards = 32): num_shards(shards),buckets(shards)
        {}

        // Exclusive access to lock
        void set(const std::string& key,const std::string& value)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);
            buckets[idx].store[key] = value; 
        }

        // No exclusive access to lock
        std::optional <std::string> get(const std::string& key) const
        {
            size_t idx = get_bucket_index(key);
            std::shared_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);
            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                return it -> second;
            }
            return std::nullopt;
        }

        // Exclusive access to lock
        bool remove(const std::string& key)
        {
            size_t idx = get_bucket_index(key);
            std::unique_lock <std::shared_mutex> lock(buckets[idx].rw_mutex);
            return buckets[idx].store.erase(key) > 0;
        }
};
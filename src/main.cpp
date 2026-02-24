#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <mutex>
#include <vector>
#include <functional>

using namespace std;

// Individual bucket structure encapsulating a map and its specific lock
struct KVPairBucket {
    unordered_map <string,string> store;
    mutable shared_mutex rw_mutex;      // so that const methods can lock
};

class ShardedKVStore {
    private:
        vector<KVPairBucket> buckets;
        size_t num_shards;

        // Routing a key to a bucket
        size_t get_bucket_index(const string& key) const
        {
            return hash<string>{}(key) % num_shards;
        }
    
    public:
        explicit ShardedKVStore(size_t shards = 32): num_shards(shards),buckets(shards)
        {}

        // Exclusive access to lock
        void set(const string& key,const string& value)
        {
            size_t idx = get_bucket_index(key);
            unique_lock <shared_mutex> lock(buckets[idx].rw_mutex);
            buckets[idx].store[key] = value; 
        }

        // No exclusive access to lock
        optional <string> get(const string& key) const
        {
            size_t idx = get_bucket_index(key);
            shared_lock <shared_mutex> lock(buckets[idx].rw_mutex);
            auto it = buckets[idx].store.find(key);
            if(it != buckets[idx].store.end())
            {
                return it -> second;
            }
            return nullopt;
        }

        // Exclusive access to lock
        bool remove(const string& key)
        {
            size_t idx = get_bucket_index(key);
            unique_lock <shared_mutex> lock(buckets[idx].rw_mutex);
            return buckets[idx].store.erase(key) > 0;
        }
};

int main()
{

}
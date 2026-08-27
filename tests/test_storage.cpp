#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "../include/storage/ShardedKVStore.hpp"

void test_basic_set_get()
{
    ShardedKVStore store(4, 10);
    
    store.set("apple", "red");
    auto val = store.get("apple");
    
    assert(val.has_value());
    assert(*val == "red");
    std::cout << "Basic SET/GET test passed.\n";
}

void test_ttl_expiration()
{
    ShardedKVStore store(4, 10);
    
    // Set a key with a 1-second TTL
    store.set("temp_key", "temp_val", 1);
    
    // Should exist immediately
    auto val1 = store.get("temp_key");
    assert(val1.has_value());
    
    // Wait for TTL to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Should be gone
    auto val2 = store.get("temp_key");
    assert(!val2.has_value());
    std::cout << "TTL expiration test passed.\n";
}

void test_lru_eviction()
{
    // Small capacity of 2 per shard to force eviction quickly
    ShardedKVStore store(1, 2);
    
    store.set("k1", "v1");
    store.set("k2", "v2");
    
    // Access k1 so k2 becomes the least recently used item
    store.get("k1");
    
    // Inserting k3 should evict k2
    store.set("k3", "v3");
    
    assert(store.get("k1").has_value());
    assert(!store.get("k2").has_value()); // Evicted
    assert(store.get("k3").has_value());
    std::cout << "LRU eviction test passed.\n";
}

int main()
{
    std::cout << "Running Storage Layer & Custom Allocator Tests...\n";
    test_basic_set_get();
    test_ttl_expiration();
    test_lru_eviction();
    std::cout << "All storage tests passed successfully.\n";
    return 0;
}
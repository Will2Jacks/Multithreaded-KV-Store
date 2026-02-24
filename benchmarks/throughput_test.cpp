#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include "../include/storage/ShardedKVStore.hpp"

constexpr int NUM_THREADS = 8;
constexpr int OPS_PER_THREAD = 100000;

void benchmark_worker(ShardedKVStore& store,int thread_id,std::atomic<int>& total_ops)
{
    for(int i = 0;i < OPS_PER_THREAD;i++)
    {
        std::string key = "key_" + std::to_string(thread_id) + "_" + std::to_string(i);

        store.set(key,"benchmark_value");
        store.get(key);
    }
    total_ops += (OPS_PER_THREAD) * 2;
}

int main()
{
    ShardedKVStore store(32,50000);
    std::vector<std::thread> threads;
    std::atomic<int> total_ops{0};

    std::cout << "Starting benchmark with " << NUM_THREADS << " threads...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    for(int i = 0;i < NUM_THREADS;i++)
    {
        threads.emplace_back(benchmark_worker,std::ref(store),i,std::ref(total_ops));
    }

    for(auto& t:threads)
    {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    double ops_per_sec = total_ops.load() / duration.count();

    std::cout << "Total Operations: " << total_ops.load() << "\n";
    std::cout << "Time elapsed: " << duration.count() << " seconds\n";
    std::cout << "Throughput: " << ops_per_sec << " ops/sec\n";

    return 0;
}
# High-Performance In-Memory Key-Value Store

A highly concurrent, low-latency in-memory key-value store written from scratch in modern C++17. Designed to handle massive concurrent network traffic, this project implements a custom event-driven network layer, a thread-safe storage engine, and a custom memory pool.

## Core Architecture

* **Asynchronous Networking (epoll):** Utilizes Linux epoll with EPOLLONESHOT for non-blocking, event-driven network I/O, allowing the server to handle thousands of concurrent connections efficiently without thread-per-connection overhead.
* **Custom Thread Pool:** Implements a producer-consumer thread pool using std::condition_variable to dispatch active socket events to background worker threads, eliminating CPU spin-locking.
* **Sharded Storage Engine:** Bypasses global lock bottlenecks by sharding the underlying hash map into independent buckets.
* **Concurrency Control:** Utilizes C++17 std::shared_mutex (Reader-Writer locks) to allow unlimited concurrent reads while strictly protecting exclusive write operations.
* **O(1) LRU Eviction:** Integrates a Least Recently Used (LRU) cache eviction policy using a doubly linked list to enforce strict memory limits.
* **Custom Slab Memory Allocator:** Features a pre-allocated 1GB contiguous memory block sliced into fixed-size chunks managed via a thread-safe free-list, entirely bypassing operating system memory fragmentation and slow malloc/free calls for standard map nodes.

## Project Directory Structure

```text
kv-store/
├── benchmarks/
│   ├── network_test.cpp
│   └── throughput_test.cpp
├── include/
│   ├── network/
│   │   ClientContext.hpp
│   │   └── SocketUtils.hpp
│   ├── server/
│   │   ├── ClientHandler.hpp
│   │   └── ThreadPool.hpp
│   ├── storage/
│   │   └── ShardedKVStore.hpp
│   └── utils/
│       └── SlabAllocator.hpp
├── src/
│   ├── main.cpp
│   ├── network/
│   │   └── SocketUtils.cpp
│   └── server/
│       └── ClientHandler.cpp
├── tests/
│   ├── test_network.cpp
│   └── test_storage.cpp
└── scripts/
    └── build.sh
```

## Build Instructions

To compile the optimized server executable manually, run:

g++ -std=c++17 -O3 src/main.cpp src/server/ClientHandler.cpp src/network/SocketUtils.cpp -pthread -o kv_server

## Running the Server

To start the server on default port 8080, run:

./kv_server

You can interact with the server using standard TCP utilities like nc or telnet via custom text commands:

echo "SET mykey 123" | nc 127.0.0.1 8080
echo "GET mykey" | nc 127.0.0.1 8080
echo "SETEX temp_key 5 secret_val" | nc 127.0.0.1 8080
echo "DEL mykey" | nc 127.0.0.1 8080

## Testing Suite

To compile and execute the unit and integration tests:

Storage Layer & Custom Allocator Tests:
g++ -std=c++17 tests/test_storage.cpp -pthread -o test_storage && ./test_storage

Network Layer Tests:
g++ -std=c++17 tests/test_network.cpp src/network/SocketUtils.cpp -o test_network && ./test_network

## Performance Benchmarks

The project includes custom benchmarking suites to measure raw storage throughput and network I/O efficiency.

* **Storage Throughput:** Achieved ~2.58 Million Operations/Second executing highly concurrent reads and writes across 8 threads, demonstrating the efficiency of the lock-striping architecture and custom slab memory allocator.

* **Network Throughput:** Achieved high-performance request handling processing full TCP connection cycles across concurrent clients.

(Tested on local machine using the included benchmark executables compiled with -O3 and -march=native).

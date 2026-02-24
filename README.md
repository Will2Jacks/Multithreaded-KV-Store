# High-Performance In-Memory Key-Value Store

A highly concurrent, low-latency in-memory key-value store written from scratch in modern C++17. Designed to handle massive concurrent network traffic, this project implements a custom event-driven network layer and a highly optimized, thread-safe storage engine.

## Core Architecture

* **Asynchronous Networking (`epoll`):** Utilizes Linux `epoll` with `EPOLLONESHOT` for non-blocking, event-driven network I/O, allowing the server to handle thousands of concurrent connections efficiently without thread-per-connection overhead.
* **Custom Thread Pool:** Implements a producer-consumer thread pool using `std::condition_variable` to dispatch active socket events to background worker threads, eliminating CPU spin-locking.
* **Sharded Storage Engine:** Bypasses global lock bottlenecks by sharding the underlying hash map into independent buckets.
* **Concurrency Control:** Utilizes C++17 `std::shared_mutex` (Reader-Writer locks) to allow unlimited concurrent reads while strictly protecting exclusive write operations.
* **O(1) LRU Eviction:** Integrates a Least Recently Used (LRU) cache eviction policy using a doubly linked list to enforce strict memory limits.

## Build Instructions

This project uses CMake and requires a compiler supporting C++17.

```bash
# Generate the build files
cmake -B build

# Compile the optimized executable
cmake --build build

```

## Running the Server

```bash
# Start the server on default port 8080:
./build/kvserver

```

You can interact with the server using standard TCP utilities like nc or via telnet using RESP-like text commands:

```bash
echo "SET mykey 123" | nc 127.0.0.1 8080
echo "GET mykey" | nc 127.0.0.1 8080
echo "DEL mykey" | nc 127.0.0.1 8080
```

## Performance Benchmarks
The project includes custom benchmarking suites to measure raw storage throughput and network I/O efficiency.

* **Storage Throughput:** Achieved ~2.58 Million Operations/Second executing highly concurrent reads and writes across 8 threads, demonstrating the efficiency of the lock-striping architecture.

* **Network Throughput:** Achieved ~44,000 Requests/Second processing full TCP connection churn (connect, send, read, close) across concurrent clients.

(Tested on local machine using the included `benchmark_store` and `network_test` executables compiled with `-O3` and `-march=native`).
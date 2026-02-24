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
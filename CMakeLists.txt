# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(KVStore LANGUAGES CXX)

# Enforce C++17 standard for std::shared_mutex and std::optional
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -O3 turns on the most aggressive compiler optimizations (loop unrolling, heavy inlining)
# -march=native optimizes the binary specifically for the CPU architecture of your current machine
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra")

# Locate the system's POSIX thread library
find_package(Threads REQUIRED)

# Define the final executable and its source files
add_executable(kvserver src/main.cpp)

# Link the threading library to the executable
target_link_libraries(kvserver Threads::Threads)
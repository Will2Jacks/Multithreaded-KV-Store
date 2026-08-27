#!/bin/bash
echo "Building High-Performance KV Server..."
mkdir -p build
cd build
cmake .. 2>/dev/null || g++ -std=c++17 ../src/main.cpp ../src/server/ClientHandler.cpp ../src/network/SocketUtils.cpp -pthread -o kv_server
echo "Build complete. Executable ready in build directory."
#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "../storage/ShardedKVStore.hpp"
#include "../network/ClientContext.hpp"

extern std::unordered_map <int,ClientContext> client_contexts;
extern std::mutex contexts_mutex;

void handle_client(int client_socket,ShardedKVStore& store,int epoll_fd,uint32_t events);
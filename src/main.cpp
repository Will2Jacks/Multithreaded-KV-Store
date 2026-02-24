#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "../include/storage/ShardedKVStore.hpp"
#include "../include/server/ThreadPool.hpp"

constexpr int PORT = 8080;

// For worker_thread
void handle_client(int client_socket,ShardedKVStore& store)
{
    char buffer[1024] = {0};
    
    read(client_socket,buffer,1024);

    std::string response = "+PONG\r\n";
    send(client_socket,response.c_str(),response.length(),0);

    close(client_socket);
}

int main()
{
    ShardedKVStore store(32);
    ThreadPool pool(4);         // 4 background worker threads

    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if(server_fd <= 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT,&opt,sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(server_fd,(struct sockaddr*) &address,sizeof(address)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    if(listen(server_fd,10) < 0)
    {
        perror("Listen failed");
        return 1;
    }

    std::cout << "KV Server listening on port " << PORT << "...\n";

    while(true)
    {
        int client_socket = accept(server_fd,nullptr,nullptr);
        if(client_socket < 0)
        {
            continue;
        }

        pool.enqueue([client_socket,&store]() {
            handle_client(client_socket,store);
        });
    }

    return 0;
}
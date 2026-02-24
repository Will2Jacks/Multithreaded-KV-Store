#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sstream>
#include "../include/storage/ShardedKVStore.hpp"
#include "../include/server/ThreadPool.hpp"

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 1024;

// Set sockets to non-blocking mode
int set_nonblocking(int fd)
{
    int flags = fcntl(fd,F_GETFL,0);
    if(flags == -1) return -1;
    return fcntl(fd,F_SETFL,flags | O_NONBLOCK);
}

// For worker_thread
void handle_client(int client_socket,ShardedKVStore& store)
{
    char buffer[1024] = {0};
    ssize_t bytes_read = read(client_socket,buffer,sizeof(buffer) - 1);

    if(bytes_read <= 0)
    {
        close(client_socket);
        return;
    }

    std::string request(buffer);
    std::istringstream iss(request);
    std::string command,key,value,response;

    iss >> command;

    // Route to the appropriate ShardedKVStore method
    if(command == "SET")
    {
        iss >> key >> value;
        store.set(key,value);
        response = "+OK\r\n"; // Simple string response
    }
    else if(command == "GET")
    {
        iss >> key;
        auto result = store.get(key);
        if(result)
        {
            // Bulk string response format: $<length>\r\n<data>\r\n
            response = "$" + std::to_string(result -> length()) + "\r\n" + *result + "\r\n";
        }
        else
        {
            response = "$-1\r\n";
        }
    }
    else if(command == "DEL")
    {
        iss >> key;
        bool removed = store.remove(key);
        // Integer response format: :<number>\r\n
        response = ":" + std::to_string(removed) + "\r\n";
    }
    else
    {
        response = "-ERR unknown command\r\n";
    }

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
    set_nonblocking(server_fd);

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

    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1)
    {
        perror("epoll_create1 failed");
        return 1;
    }

    struct epoll_event event;
    event.data.fd = server_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_fd,&event);

    struct epoll_event events[MAX_EVENTS];
    std::cout << "High-Performance KV Server listening on port " << PORT << "...\n";

    while(true)
    {
        int num_events = epoll_wait(epoll_fd,events,MAX_EVENTS,-1);

        for(int i = 0;i < num_events;i++)
        {
            if(events[i].data.fd == server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_socket = accept(server_fd,(struct sockaddr*)& client_addr,&client_len);

                if(client_socket >= 0)
                {
                    set_nonblocking(client_socket);
                    struct epoll_event client_event;
                    client_event.data.fd = client_socket;

                    // EPOLLONESHOT ensures a socket is only triggered once. 
                    // This prevents multiple worker threads from grabbing the same incoming data simultaneously.

                    client_event.events = EPOLLIN | EPOLLONESHOT;
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_socket,&client_event);
                }
            }
            else
            {
                // An existing client has sent data (GET, SET, DEL)
                int client_socket = events[i].data.fd;

                pool.enqueue([client_socket,&store]() {
                    handle_client(client_socket,store);
                });
            }
        }
    }

    return 0;
}
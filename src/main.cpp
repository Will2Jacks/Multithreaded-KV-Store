#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <csignal>
#include <atomic>
#include "../include/storage/ShardedKVStore.hpp"
#include "../include/server/ThreadPool.hpp"
#include "../include/network/SocketUtils.hpp"
#include "../include/network/ClientContext.hpp"
#include "../include/server/ClientHandler.hpp"

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 1024;

std::atomic<bool> server_running{true};

void signal_handler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Initiating graceful shutdown...\n";
    server_running = false;
}

int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ShardedKVStore store(32);
    ThreadPool pool(4);

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

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if(timer_fd == -1) 
    {
        perror("timerfd_create failed");
        return 1;
    }

    struct itimerspec ts;
    ts.it_value.tv_sec = 5;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 5;
    ts.it_interval.tv_nsec = 0;
    timerfd_settime(timer_fd, 0, &ts, nullptr);

    struct epoll_event timer_event;
    timer_event.data.fd = timer_fd;
    timer_event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &timer_event);

    while(server_running)
    {
        int num_events = epoll_wait(epoll_fd,events,MAX_EVENTS,-1);

        if(num_events == -1 && errno == EINTR) continue;

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

                    {
                        std::lock_guard <std::mutex> lock(contexts_mutex);
                        client_contexts[client_socket] = ClientContext();
                    }

                    struct epoll_event client_event;
                    client_event.data.fd = client_socket;
                    client_event.events = EPOLLIN | EPOLLONESHOT;
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_socket,&client_event);
                }
            }
            else if(events[i].data.fd == timer_fd)
            {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) continue;

                pool.enqueue([&store](){
                    store.clean_expired_keys();
                });
            }
            else
            {
                int client_socket = events[i].data.fd;
                uint32_t triggered_events = events[i].events;
                pool.enqueue([client_socket,&store,epoll_fd,triggered_events]() {
                    handle_client(client_socket,store,epoll_fd,triggered_events);
                });
            }
        }
    }

    std::cout << "Server shutting down cleanly.\n";
    close(server_fd);
    close(epoll_fd);
    close(timer_fd);
    return 0;
}
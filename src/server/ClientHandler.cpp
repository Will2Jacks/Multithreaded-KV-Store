#include "../../include/server/ClientHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <sys/epoll.h>

std::unordered_map <int,ClientContext> client_contexts;
std::mutex contexts_mutex;

void handle_client(int client_socket,ShardedKVStore& store,int epoll_fd,uint32_t events)
{

    if(events & EPOLLOUT)
    {
        std::lock_guard <std::mutex> lock(contexts_mutex);
        auto& ctx = client_contexts[client_socket];

        ssize_t bytes_sent = send(client_socket,ctx.write_buffer.c_str(),ctx.write_buffer.length(),0);

        if(bytes_sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_contexts.erase(client_socket);
            close(client_socket);
            return;
        }

        if(bytes_sent > 0)
        {
            ctx.write_buffer.erase(0,bytes_sent);
        }

        struct epoll_event event;
        event.data.fd = client_socket;

        if(ctx.write_buffer.empty())
        {
            event.events = EPOLLIN | EPOLLONESHOT;
        }
        else
        {
            event.events = EPOLLOUT | EPOLLONESHOT;
        }
        
        epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket,&event);
        return;
    }

    char buffer[1024] = {0};
    ssize_t bytes_read = read(client_socket,buffer,sizeof(buffer) - 1);

    if(bytes_read <= 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::lock_guard <std::mutex> lock(contexts_mutex);
        client_contexts.erase(client_socket);
        close(client_socket);
        return;
    }

    std::string current_request;
    {
        std::lock_guard <std::mutex> lock(contexts_mutex);
        
        if(bytes_read > 0) 
        {
            client_contexts[client_socket].read_buffer.append(buffer,bytes_read);
        }

        size_t pos = client_contexts[client_socket].read_buffer.find("\r\n");
        size_t delim_len = 2;

        if(pos == std::string::npos)
        {
            pos = client_contexts[client_socket].read_buffer.find("\n");
            delim_len = 1;
        }

        if(pos == std::string::npos)
        {
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLONESHOT;
            event.data.fd = client_socket;
            epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket,&event);
            return;
        }

        current_request = client_contexts[client_socket].read_buffer.substr(0,pos);
        client_contexts[client_socket].read_buffer.erase(0,pos + delim_len);
    }

    std::istringstream iss(current_request);
    std::string command,key,value,response;

    iss >> command;

    if(command == "SET")
    {
        iss >> key >> value;
        store.set(key,value);
        response = "+OK\r\n";
    }
    else if(command == "SETEX")
    {
        int ttl;
        iss >> key >> ttl >> value;
        store.set(key, value, ttl);
        response = "+OK\r\n";
    }
    else if(command == "GET")
    {
        iss >> key;
        auto result = store.get(key);
        if(result)
        {
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
        response = ":" + std::to_string(removed) + "\r\n";
    }
    else
    {
        response = "-ERR unknown command\r\n";
    }

    {
        std::lock_guard <std::mutex> lock(contexts_mutex);
        auto& ctx = client_contexts[client_socket];
        ctx.write_buffer += response;

        ssize_t bytes_sent = send(client_socket,ctx.write_buffer.c_str(),ctx.write_buffer.length(),0);

        if(bytes_sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            client_contexts.erase(client_socket);
            close(client_socket);
            return;
        }

        if(bytes_sent > 0)
        {
            ctx.write_buffer.erase(0,bytes_sent);
        }

        struct epoll_event event;
        event.data.fd = client_socket;

        if(ctx.write_buffer.empty())
        {
            event.events = EPOLLIN | EPOLLONESHOT;
        }
        else
        {
            event.events = EPOLLOUT | EPOLLONESHOT;
        }
        
        epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_socket,&event);
    }
}
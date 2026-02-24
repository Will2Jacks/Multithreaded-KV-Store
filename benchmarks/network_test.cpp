#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

constexpr int PORT = 8080;
constexpr int NUM_THREADS = 8;
constexpr int REQS_PER_THREAD = 5000;

void network_worker(int thread_id,std::atomic<int>& successful_reqs)
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET,"127.0.0.1",&serv_addr.sin_addr);

    std::string command = "GET test_key\r\n";
    char buffer[256];

    for(int i = 0;i < REQS_PER_THREAD;i++)
    {
        int sock = socket(AF_INET,SOCK_STREAM,0);
        if(sock < 0) continue;

        if(connect(sock,(struct sockaddr*)& serv_addr,sizeof(serv_addr)) == 0)
        {
            if(send(sock,command.c_str(),command.length(),0) > 0)
            {
                if(read(sock,buffer,sizeof(buffer) - 1) > 0)
                {
                    successful_reqs++;
                }
            }
        }
        close(sock);
    }
}

int main()
{
    std::vector<std::thread> threads;
    std::atomic<int> successful_reqs{0};

    std::cout << "Starting network benchmark with " << NUM_THREADS << " threads...\n";
    std::cout << "Testing TCP connection churn and epoll event dispatching...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    for(int i = 0;i < NUM_THREADS;i++)
    {
        threads.emplace_back(network_worker,i,std::ref(successful_reqs));
    }

    for(auto& t:threads)
    {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    double rps = successful_reqs.load() / duration.count();

    std::cout << "Successful Network Requests: " << successful_reqs.load() << "\n";
    std::cout << "Time elapsed: " << duration.count() << " seconds\n";
    std::cout << "Network Throughput: " << rps << " requests/sec\n";

    return 0;
}
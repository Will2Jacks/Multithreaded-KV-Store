#include <iostream>
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "../include/network/SocketUtils.hpp"
#include "../include/network/ClientContext.hpp"

void test_nonblocking_socket()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int result = set_nonblocking(sockfd);
    assert(result != -1);

    close(sockfd);
    std::cout << "Socket utils tests passed successfully.\n";
}

void test_client_context()
{
    ClientContext ctx;
    ctx.read_buffer = "SET key val\r\n";
    ctx.write_buffer = "+OK\r\n";

    assert(ctx.read_buffer == "SET key val\r\n");
    assert(ctx.write_buffer == "+OK\r\n");
    std::cout << "Client context tests passed successfully.\n";
}

int main()
{
    std::cout << "Running Network Layer Tests...\n";
    test_nonblocking_socket();
    test_client_context();
    std::cout << "All network tests passed.\n";
    return 0;
}
#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <iostream>

class Socket {
public:
    explicit Socket(int domain, int type, int protocol) {
        sockfd_ = ::socket(domain, type, protocol);
        // std::cout << "Socket initialized [new]\n";
    }
    explicit Socket(int sockfd) 
            : sockfd_(sockfd) {
        // std::cout << "Socket initialized [existing]\n";
    }
    ~Socket() {     
        if (sockfd_ >= 0) ::close(sockfd_);
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept 
            : sockfd_(std::exchange(other.sockfd_, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (sockfd_ >= 0) ::close(sockfd_);
            sockfd_ = std::exchange(other.sockfd_, -1);
        }
        return *this;
    }

    int get() const { return sockfd_; }

private:
    int sockfd_{-1};
};


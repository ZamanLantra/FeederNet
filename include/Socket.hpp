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
#include <netdb.h>
#include <cstring>

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

namespace utils {

std::string resolveDockerIP(const std::string hostname = "my-server") {
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
    if (status != 0 || !res)
        throw std::runtime_error("getaddrinfo failed: " + std::string(gai_strerror(status)));

    char ipStr[INET_ADDRSTRLEN];
    auto* ipv4 = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &ipv4->sin_addr, ipStr, sizeof(ipStr));

    freeaddrinfo(res);

    std::cout << "Resolved IP for " << hostname << ": " << ipStr << std::endl;
    return std::string(ipStr);
}

} // namespace utils
#include "socket_utils.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void closeSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int connectToServer(const std::string& host, int port) {
    int fd{socket(AF_INET, SOCK_STREAM, 0)};

    if (fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "invalid address: " << host << '\n';
        closeSocket(fd);
        return -1;
    }

    if (connect(fd,
                reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) < 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << '\n';
        closeSocket(fd);
        return -1;
    }

    return fd;
}

bool sendAll(int fd, const std::string& message) {
    const char* data{message.c_str()};
    std::size_t totalSent{0};
    std::size_t totalSize{message.size()};

    while (totalSent < totalSize) {
        ssize_t bytes_sent{
            send(fd, data + totalSent, totalSize - totalSent, 0)
        };

        if (bytes_sent < 0) {
            return false;
        }

        if (bytes_sent == 0) {
            return false;
        }

        totalSent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

std::optional<std::string> readMessage(int fd) {
    std::string message{};
    char ch{};

    while (true) {
        ssize_t bytesReceived{recv(fd, &ch, 1, 0)};

        if (bytesReceived <= 0) {
            return std::nullopt;
        }

        if (ch == '\n') {
            return message;
        }

        message += ch;
    }
}

#include "SocketUtils.h"

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

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "invalid address: " << host << '\n';
        closeSocket(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << '\n';
        closeSocket(fd);
        return -1;
    }

    return fd;
}

bool sendAll(int fd, const std::vector<std::uint8_t>& message) {
    const auto* data{reinterpret_cast<const char*>(message.data())};
    std::size_t totalSent{0};

    while (totalSent < message.size()) {
        ssize_t bytesSent{send(fd, data + totalSent, message.size() - totalSent, 0)};
        if (bytesSent <= 0) {
            return false;
        }

        totalSent += static_cast<std::size_t>(bytesSent);
    }

    return true;
}

// Reads exactly "byteCount" bytes from the given socket into the given buffer.
bool readExact(int fd, void* buffer, std::size_t byteCount) {
    auto* out{reinterpret_cast<char*>(buffer)};
    std::size_t totalRead{0};

    while (totalRead < byteCount) {
        ssize_t bytesRead{recv(fd, out + totalRead, byteCount - totalRead, 0)};
        if (bytesRead <= 0) {
            return false;
        }

        totalRead += static_cast<std::size_t>(bytesRead);
    }

    return true;
}

// Receive a message on the socket, extract its length, then read
// that many byte into a buffer.
std::optional<std::vector<std::uint8_t>> readMessage(int fd) {
    std::uint32_t networkLength{};
    if (!readExact(fd, &networkLength, sizeof(networkLength))) {
        return std::nullopt;
    }

    const std::uint32_t payloadLength{ntohl(networkLength)};
    if (payloadLength == 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> payload(payloadLength);
    if (!readExact(fd, payload.data(), payload.size())) {
        return std::nullopt;
    }

    return payload;
}

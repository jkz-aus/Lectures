#include "request_handlers.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

void closeSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
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

std::optional<std::vector<std::uint8_t>> readMessage(int fd) {
    std::uint32_t networkLength{};
    if (!readExact(fd, &networkLength, sizeof(networkLength))) {
        return std::nullopt;
    }

    const std::uint32_t length{ntohl(networkLength)};
    if (length == 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> payload(length);
    if (!readExact(fd, payload.data(), payload.size())) {
        return std::nullopt;
    }

    return payload;
}

bool appendInt32(std::vector<std::uint8_t>& payload, std::int32_t value) {
    const std::uint32_t networkValue{htonl(static_cast<std::uint32_t>(value))};
    const auto* bytes{reinterpret_cast<const std::uint8_t*>(&networkValue)};
    payload.insert(payload.end(), bytes, bytes + sizeof(networkValue));
    return true;
}

bool appendString(std::vector<std::uint8_t>& payload, const std::string& value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    appendInt32(payload, static_cast<std::int32_t>(value.size()));
    payload.insert(payload.end(), value.begin(), value.end());
    return true;
}

std::vector<std::uint8_t> buildMessage(ResponseOpcode opcode) {
    return {static_cast<std::uint8_t>(opcode)};
}

std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode) {
    return buildMessage(opcode);
}

std::vector<std::uint8_t> buildErrorResponse(const std::string& error) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Error)};
    appendString(payload, error);
    return payload;
}

std::vector<std::uint8_t> buildValueResponse(const std::string& value) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Value)};
    appendString(payload, value);
    return payload;
}

std::vector<std::uint8_t> buildCountResponse(std::size_t count) {
    if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return buildErrorResponse("count overflow");
    }

    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Count)};
    appendInt32(payload, static_cast<std::int32_t>(count));
    return payload;
}

std::vector<std::uint8_t> frameMessage(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> framed{};
    framed.reserve(sizeof(std::uint32_t) + payload.size());

    const std::uint32_t networkLength{htonl(static_cast<std::uint32_t>(payload.size()))};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

std::string opcodeName(RequestOpcode opcode) {
    switch (opcode) {
        case RequestOpcode::Push:
            return "PUSH";
        case RequestOpcode::Pop:
            return "POP";
        case RequestOpcode::Insert:
            return "INSERT";
        case RequestOpcode::Remove:
            return "REMOVE";
        case RequestOpcode::Count:
            return "COUNT";
        case RequestOpcode::Get:
            return "GET";
        case RequestOpcode::Set:
            return "SET";
        case RequestOpcode::Swap:
            return "SWAP";
        case RequestOpcode::Clear:
            return "CLEAR";
        case RequestOpcode::Quit:
            return "QUIT";
    }

    return "UNKNOWN";
}

void handleClient(int clientFd, SharedStore& store) {
    while (true) {
        std::optional<std::vector<std::uint8_t>> payload{readMessage(clientFd)};
        if (!payload.has_value()) {
            std::cout << "Client disconnected\n";
            break;
        }

        if (payload->empty()) {
            std::cout << "Received empty payload\n";
            break;
        }

        const auto opcodeValue{payload->front()};
        const auto opcode{static_cast<RequestOpcode>(opcodeValue)};
        std::vector<std::uint8_t> arguments{payload->begin() + 1, payload->end()};
        std::vector<std::uint8_t> response{handleRequest(opcode, arguments, store)};

        std::cout << "request opcode: " << opcodeName(opcode)
                  << " (" << static_cast<int>(opcodeValue) << ")\n";

        if (!sendAll(clientFd, frameMessage(response))) {
            std::cout << "Failed to send response\n";
            break;
        }

        if (!response.empty() && response.front() == static_cast<std::uint8_t>(ResponseOpcode::Bye)) {
            break;
        }
    }

    closeSocket(clientFd);
}

int createServerSocket(int port) {
    int serverFd{socket(AF_INET, SOCK_STREAM, 0)};

    if (serverFd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        std::exit(1);
    }

    int yes{1};
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    return serverFd;
}

int main() {
    constexpr int PORT{9090};

    int serverFd{createServerSocket(PORT)};
    SharedStore store{};

    std::cout << "Binary list server listening on port " << PORT << '\n';

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen{sizeof(clientAddr)};

        int clientFd{
            accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen)
        };

        if (clientFd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        char clientIp[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

        std::cout << "Accepted client from "
                  << clientIp
                  << ':'
                  << ntohs(clientAddr.sin_port)
                  << '\n';

        std::thread clientThread{handleClient, clientFd, std::ref(store)};
        clientThread.detach();
    }

    closeSocket(serverFd);
    return 0;
}

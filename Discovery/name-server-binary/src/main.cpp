#include "MessageReader.h"
#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include "SharedRegistry.h"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        MessageReader& reader,
                                        SharedRegistry& registry);

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

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        MessageReader& reader,
                                        SharedRegistry& registry) {
    switch (opcode) {
        case RequestOpcode::Register:
            return handleRegisterRequest(reader, registry);
        case RequestOpcode::Unregister:
            return handleUnregisterRequest(reader, registry);
        case RequestOpcode::Heartbeat:
            return handleHeartbeatRequest(reader, registry);
        case RequestOpcode::Resolve:
            return handleResolveRequest(reader, registry);
        case RequestOpcode::Quit:
            return handleQuitRequest(reader, registry);
    }

    return buildErrorResponse("unknown opcode");
}

std::vector<std::uint8_t> frameResponse(const std::vector<std::uint8_t>& payload) {
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
        case RequestOpcode::Register:
            return "REGISTER";
        case RequestOpcode::Unregister:
            return "UNREGISTER";
        case RequestOpcode::Heartbeat:
            return "HEARTBEAT";
        case RequestOpcode::Resolve:
            return "RESOLVE";
        case RequestOpcode::Quit:
            return "QUIT";
    }

    return "UNKNOWN";
}

void handleClient(int clientFd, std::int32_t clientId, SharedRegistry& registry) {
    std::int32_t lastMessage{0};
    std::vector<std::uint8_t> lastResponsePayload{};

    if (!sendAll(clientFd, frameResponse(buildClientIdResponse(clientId)))) {
        closeSocket(clientFd);
        return;
    }

    while (true) {
        std::optional<std::vector<std::uint8_t>> payload{readMessage(clientFd)};
        if (!payload.has_value() || payload->empty()) {
            break;
        }

        MessageReader reader{payload.value(), 0};
        std::optional<std::int32_t> requestClientId{reader.readInt32()};
        std::optional<std::int32_t> messageId{reader.readInt32()};
        if (!requestClientId.has_value() || !messageId.has_value()) {
            sendAll(clientFd, frameResponse(buildErrorResponse("missing clientId or messageId")));
            break;
        }

        if (requestClientId.value() != clientId) {
            sendAll(clientFd, frameResponse(buildErrorResponse("invalid clientId")));
            break;
        }

        if (lastMessage == messageId.value()) {
            sendAll(clientFd, frameResponse(lastResponsePayload));
            continue;
        }

        std::optional<std::uint8_t> opcodeValue{reader.readByte()};
        if (!opcodeValue.has_value()) {
            sendAll(clientFd, frameResponse(buildErrorResponse("missing opcode")));
            break;
        }

        const auto opcode{static_cast<RequestOpcode>(opcodeValue.value())};
        std::vector<std::uint8_t> response{handleRequest(opcode, reader, registry)};

        std::cout << "request opcode: " << opcodeName(opcode)
                  << " (" << static_cast<int>(opcodeValue.value()) << ")\n";

        if (!sendAll(clientFd, frameResponse(response))) {
            break;
        }

        lastMessage = messageId.value();
        lastResponsePayload = response;

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
    constexpr int PORT{9091};
    static std::atomic<std::int32_t> nextClientId{1};

    int serverFd{createServerSocket(PORT)};
    SharedRegistry registry{};

    std::cout << "Name server listening on port " << PORT << '\n';

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

        const std::int32_t clientId{nextClientId.fetch_add(1)};
        std::thread clientThread{handleClient, clientFd, clientId, std::ref(registry)};
        clientThread.detach();
    }

    closeSocket(serverFd);
    return 0;
}

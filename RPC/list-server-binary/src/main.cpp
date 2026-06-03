#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include "SharedStore.h"
#include "MessageReader.h"

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

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        MessageReader& reader,
                                        SharedStore& store);


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

// Reads exactly "byteCount" bytes from the given socket descriptor, and writes them
// to the given buffer.
bool readExact(int fd, void* buffer, std::size_t byteCount) {
    // Cast the buffer to a char* so we can store one byte at a time.
    auto* out{reinterpret_cast<char*>(buffer)};
    std::size_t totalRead{0};

    // Keep reading into the buffer (via the "out" pointer) until we have
    // received the expected number of bytes.
    while (totalRead < byteCount) {
        ssize_t bytesRead{recv(fd, out + totalRead, byteCount - totalRead, 0)};
        if (bytesRead <= 0) {
            return false;
        }

        totalRead += static_cast<std::size_t>(bytesRead);
    }

    return true;
}

// The application protocol for this server defines a message as being a sequence of
// at least 5 bytes:
// byte 0-3: the number of bytes in the message payload, which follows the first 4 bytes.
// byte 4:   a 1-byte opcode.
// byte 5+:  optional arguments appropriate to the opcode.
// This function returns the payload of a message received on the given socket.
std::optional<std::vector<std::uint8_t>> readMessage(int fd) {
    // Read the payload length.
    std::uint32_t networkLength{};
    if (!readExact(fd, &networkLength, sizeof(networkLength))) {
        return std::nullopt;
    }
    const std::uint32_t length{ntohl(networkLength)};
    if (length == 0) {
        return std::nullopt;
    }

    // Copy "length" bytes from the socket into this vector of bytes.
    std::vector<std::uint8_t> payload(length);
    if (!readExact(fd, payload.data(), payload.size())) {
        return std::nullopt;
    }

    return payload;
}

// Handles the given opcode request, validating arguments from the given
// MessageReader, and mutating the given SharedStore. Returns a binary message
// payload to respond to the client.
std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        MessageReader& reader,
                                        SharedStore& store) {
    switch (opcode) {
        case RequestOpcode::Push:
            return handlePushRequest(reader, store);
        case RequestOpcode::Pop:
            return handlePopRequest(reader, store);
        case RequestOpcode::Insert:
            return handleInsertRequest(reader, store);
        case RequestOpcode::Remove:
            return handleRemoveRequest(reader, store);
        case RequestOpcode::Count:
            return handleCountRequest(reader, store);
        case RequestOpcode::Get:
            return handleGetRequest(reader, store);
        case RequestOpcode::Set:
            return handleSetRequest(reader, store);
        case RequestOpcode::Swap:
            return handleSwapRequest(reader, store);
        case RequestOpcode::Clear:
            return handleClearRequest(reader, store);
        case RequestOpcode::Quit:
            return handleQuitRequest(reader, store);
    }

    return buildErrorResponse("unknown opcode");
}

// Frame a payload of bytes as a network message, prefixed with the payload's length
std::vector<std::uint8_t> frameResponse(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> framed{};
    // Reserve enough space in the new buffer.
    framed.reserve(sizeof(std::uint32_t) + payload.size());

    // Encode the length of the payload.
    const std::uint32_t networkLength{htonl(static_cast<std::uint32_t>(payload.size()))};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};
    // Copy the length to the buffer.
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
    // Copy the payload to the buffer.
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

// Convenient debugging method. I wish C++ could do this on its own.
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
        // Read a message from the client,
        // to receive back a buffer of bytes from the message payload.
        std::optional<std::vector<std::uint8_t>> payload{readMessage(clientFd)};
        if (!payload.has_value()) {
            std::cout << "Client disconnected\n";
            break;
        }

        if (payload->empty()) {
            std::cout << "Received empty payload\n";
            break;
        }
        MessageReader reader{payload.value(), 0};
        // Retrieve the opcode from the payload.
        const auto opcodeValue{reader.readByte()};
        const auto opcode{static_cast<RequestOpcode>(opcodeValue.value())};

        std::vector<std::uint8_t> response{handleRequest(opcode, reader, store)};

        std::cout << "request opcode: " << opcodeName(opcode)
                  << " (" << static_cast<int>(opcodeValue.value()) << ")\n";

        if (!sendAll(clientFd, frameResponse(response))) {
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

    // As before, accept a connection on the socket, then spawn a thread to handle the client.
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

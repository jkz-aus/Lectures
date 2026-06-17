#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include "SharedStore.h"
#include "MessageReader.h"
#include "Messaging.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <functional>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <system_error>
#include <charconv>
#include <vector>

#include "NameServerHandlers.h"

std::optional<std::vector<std::uint8_t>> readMessage(int fd);
bool sendAll(int fd, const std::vector<std::uint8_t>& message);
void closeSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int connectToServer(const std::string& host, int port) {
    int fd{socket(AF_INET, SOCK_STREAM, 0)};
    if (fd < 0) {
        return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        closeSocket(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        closeSocket(fd);
        return -1;
    }

    return fd;
}

std::optional<std::string> getLocalIpAddress(int fd) {
    sockaddr_in localAddr{};
    socklen_t localAddrLen{sizeof(localAddr)};
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&localAddr), &localAddrLen) < 0) {
        return std::nullopt;
    }

    char addressBuffer[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &localAddr.sin_addr, addressBuffer, sizeof(addressBuffer)) == nullptr) {
        return std::nullopt;
    }

    return std::string{addressBuffer};
}

bool parsePort(const char* text, int& port) {
    if (text == nullptr) {
        return false;
    }

    int parsedPort{};
    const char* end{text + std::strlen(text)};
    auto [ptr, error]{std::from_chars(text, end, parsedPort)};
    if (error != std::errc{} || ptr != end || parsedPort <= 0 || parsedPort > 65535) {
        return false;
    }

    port = parsedPort;
    return true;
}

bool registerWithNameServer(const std::string& nameServerHost,
                            int nameServerPort,
                            const std::string& serviceName,
                            const std::string& providerId,
                            int servicePort) {
    int fd{connectToServer(nameServerHost, nameServerPort)};
    if (fd < 0) {
        std::cerr << "Failed to connect to name server at "
                  << nameServerHost
                  << ':'
                  << nameServerPort
                  << '\n';
        return false;
    }

    std::optional<std::string> localIp{getLocalIpAddress(fd)};
    if (!localIp.has_value()) {
        std::cerr << "Failed to determine local service address for registration\n";
        closeSocket(fd);
        return false;
    }

    std::optional<std::vector<std::uint8_t>> handshakeMessage{readMessage(fd)};
    if (!handshakeMessage.has_value()) {
        std::cerr << "Failed to receive name server handshake\n";
        closeSocket(fd);
        return false;
    }

    MessageReader handshakeReader{handshakeMessage.value(), 0};
    const auto opcodeValue{handshakeReader.readByte()};
    if (!opcodeValue.has_value()) {
        std::cerr << "Name server handshake response missing opcode\n";
        closeSocket(fd);
        return false;
    }

    const auto opcode{static_cast<NameServerResponseOpcode>(opcodeValue.value())};
    if (opcode != NameServerResponseOpcode::ClientId) {
        std::cerr << "Invalid name server handshake response\n";
        closeSocket(fd);
        return false;
    }

    std::optional<std::int32_t> clientId{handshakeReader.readInt32()};
    if (!clientId.has_value() || !handshakeReader.isAtEnd()) {
        std::cerr << "Malformed client ID from name server\n";
        closeSocket(fd);
        return false;
    }

    const std::vector<std::uint8_t> registerPayload{
        buildRegisterRequest(serviceName, providerId, localIp.value(), servicePort)
    };
    if (!sendAll(fd, frameSuppressibleMessage(clientId.value(), 1, registerPayload))) {
        std::cerr << "Failed to send registration request to name server\n";
        closeSocket(fd);
        return false;
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(fd)};
    if (!responseMessage.has_value()) {
        std::cerr << "Failed to receive registration response from name server\n";
        closeSocket(fd);
        return false;
    }
    MessageReader registerReader{responseMessage.value(), 0};
    const auto registerOpcodeValue{registerReader.readByte()};
    if (!registerOpcodeValue.has_value()) {
        std::cerr << "Registration response missing opcode\n";
        closeSocket(fd);
        return false;
    }

    const auto registerOpcode{static_cast<NameServerResponseOpcode>(registerOpcodeValue.value())};
    if (registerOpcode == NameServerResponseOpcode::Ok) {
        closeSocket(fd);
        return true;
    }

    if (registerOpcode == NameServerResponseOpcode::Error) {
        std::optional<std::string> errorMessage{registerReader.readString()};
        std::cerr << "Name server rejected registration: "
                  << (errorMessage.has_value() ? errorMessage.value() : "unknown error")
                  << '\n';
        closeSocket(fd);
        return false;
    }

    std::cerr << "Unexpected registration response from name server\n";
    closeSocket(fd);
    return false;
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
    // Convert length from network order to local host order.
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

void handleClient(int clientFd, std::int32_t clientId, SharedStore& store) {
    std::int32_t lastMessage{0};
    std::vector<std::uint8_t> lastResponsePayload;

    if (!sendAll(clientFd, frameResponseMessage(buildClientIdResponse(clientId)))) {
        closeSocket(clientFd);
        return;
    }

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
        // Ignore the clientId for now, because one thread handles one client.
        std::optional<std::int32_t> requestClientId{reader.readInt32()};
        std::optional<std::int32_t> messageId{reader.readInt32()};
        if (!requestClientId.has_value() || !messageId.has_value()) {
            sendAll(clientFd, frameResponseMessage(buildErrorResponse("missing clientId or messageId")));
            break;
        }

        if (requestClientId.value() != clientId) {
            sendAll(clientFd, frameResponseMessage(buildErrorResponse("invalid clientId")));
            break;
        }

        if (lastMessage == messageId.value()) {
            sendAll(clientFd, frameResponseMessage(lastResponsePayload));
            continue;
        }


        // Retrieve the opcode from the payload.
        const auto opcodeValue{reader.readByte()};
        if (!opcodeValue.has_value()) {
            sendAll(clientFd, frameResponseMessage(buildErrorResponse("missing opcode")));
            break;
        }
        const auto opcode{static_cast<RequestOpcode>(opcodeValue.value())};

        std::vector<std::uint8_t> response{handleRequest(opcode, reader, store)};

        std::cout << "request opcode: " << opcodeName(opcode)
                  << " (" << static_cast<int>(opcodeValue.value()) << ")\n";

        if (!sendAll(clientFd, frameResponseMessage(response))) {
            std::cout << "Failed to send response\n";
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

int main(int argc, char* argv[]) {
    constexpr int PORT{9090};
    constexpr const char* SERVICE_NAME{"list-service"};
    static std::atomic<std::int32_t> nextClientId{1};

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <name-server-host> <name-server-port>\n";
        return 1;
    }

    int nameServerPort{};
    if (!parsePort(argv[2], nameServerPort)) {
        std::cerr << "Invalid name server port: " << argv[2] << '\n';
        return 1;
    }

    const std::string providerId{
        std::string{SERVICE_NAME} + "-" + std::to_string(static_cast<long long>(getpid()))
    };

    int serverFd{createServerSocket(PORT)};
    SharedStore store{};

    if (!registerWithNameServer(argv[1], nameServerPort, SERVICE_NAME, providerId, PORT)) {
        closeSocket(serverFd);
        return 1;
    }

    std::cout << "Binary list server listening on port " << PORT << '\n';
    std::cout << "Registered with name server at " << argv[1] << ':' << nameServerPort << '\n';

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

        const std::int32_t clientId{nextClientId.fetch_add(1)};
        std::thread clientThread{handleClient, clientFd, clientId, std::ref(store)};
        clientThread.detach();
    }

    closeSocket(serverFd);
    return 0;
}

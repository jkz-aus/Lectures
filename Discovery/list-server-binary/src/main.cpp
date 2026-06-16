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
#include <functional>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <system_error>
#include <charconv>
#include <utility>
#include <vector>

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        MessageReader& reader,
                                        SharedStore& store);

namespace {

enum class NameServerRequestOpcode : std::uint8_t {
    Register = 0x01
};

enum class NameServerResponseOpcode : std::uint8_t {
    Ok = 0x40,
    ClientId = 0x44,
    Error = 0x7F
};

struct BinaryResponse {
    NameServerResponseOpcode opcode;
    std::vector<std::uint8_t> payload;
};

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

std::vector<std::uint8_t> frameNameServerRequest(std::int32_t clientId,
                                                 std::int32_t messageId,
                                                 NameServerRequestOpcode opcode,
                                                 const std::vector<std::uint8_t>& arguments) {
    std::vector<std::uint8_t> framed{};
    const std::uint32_t payloadSize{
        static_cast<std::uint32_t>((2 * sizeof(std::int32_t)) + sizeof(std::uint8_t) + arguments.size())
    };
    const std::uint32_t networkLength{htonl(payloadSize)};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};

    framed.reserve(sizeof(networkLength) + payloadSize);
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
    appendInt32(framed, clientId);
    appendInt32(framed, messageId);
    framed.push_back(static_cast<std::uint8_t>(opcode));
    framed.insert(framed.end(), arguments.begin(), arguments.end());
    return framed;
}

std::optional<BinaryResponse> parseNameServerResponse(const std::vector<std::uint8_t>& message) {
    if (message.empty()) {
        return std::nullopt;
    }

    return BinaryResponse{
        static_cast<NameServerResponseOpcode>(message.front()),
        std::vector<std::uint8_t>(message.begin() + 1, message.end())
    };
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

    const auto closeNameServerSocket = [&]() {
        closeSocket(fd);
        fd = -1;
    };

    std::optional<std::string> localIp{getLocalIpAddress(fd)};
    if (!localIp.has_value()) {
        std::cerr << "Failed to determine local service address for registration\n";
        closeNameServerSocket();
        return false;
    }

    std::optional<std::vector<std::uint8_t>> handshakeMessage{readMessage(fd)};
    if (!handshakeMessage.has_value()) {
        std::cerr << "Failed to receive name server handshake\n";
        closeNameServerSocket();
        return false;
    }

    std::optional<BinaryResponse> handshake{parseNameServerResponse(handshakeMessage.value())};
    if (!handshake.has_value() || handshake->opcode != NameServerResponseOpcode::ClientId) {
        std::cerr << "Invalid name server handshake response\n";
        closeNameServerSocket();
        return false;
    }

    MessageReader handshakeReader{handshake->payload, 0};
    std::optional<std::int32_t> clientId{handshakeReader.readInt32()};
    if (!clientId.has_value() || !handshakeReader.isAtEnd()) {
        std::cerr << "Malformed client ID from name server\n";
        closeNameServerSocket();
        return false;
    }

    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, serviceName)
        || !appendString(arguments, providerId)
        || !appendString(arguments, localIp.value())) {
        std::cerr << "Failed to encode name server registration request\n";
        closeNameServerSocket();
        return false;
    }
    appendInt32(arguments, servicePort);

    const std::vector<std::uint8_t> request{
        frameNameServerRequest(clientId.value(), 1, NameServerRequestOpcode::Register, arguments)
    };
    if (!sendAll(fd, request)) {
        std::cerr << "Failed to send registration request to name server\n";
        closeNameServerSocket();
        return false;
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(fd)};
    if (!responseMessage.has_value()) {
        std::cerr << "Failed to receive registration response from name server\n";
        closeNameServerSocket();
        return false;
    }

    std::optional<BinaryResponse> response{parseNameServerResponse(responseMessage.value())};
    if (!response.has_value()) {
        std::cerr << "Malformed registration response from name server\n";
        closeNameServerSocket();
        return false;
    }

    if (response->opcode == NameServerResponseOpcode::Ok) {
        closeNameServerSocket();
        return true;
    }

    if (response->opcode == NameServerResponseOpcode::Error) {
        MessageReader errorReader{response->payload, 0};
        std::optional<std::string> errorMessage{errorReader.readString()};
        std::cerr << "Name server rejected registration: "
                  << (errorMessage.has_value() ? errorMessage.value() : "unknown error")
                  << '\n';
        closeNameServerSocket();
        return false;
    }

    std::cerr << "Unexpected registration response from name server\n";
    closeNameServerSocket();
    return false;
}

}


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

void handleClient(int clientFd, std::int32_t clientId, SharedStore& store) {
    std::int32_t lastMessage{0};
    std::vector<std::uint8_t> lastResponsePayload;

    if (!sendAll(clientFd, frameResponse(buildClientIdResponse(clientId)))) {
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


        // Retrieve the opcode from the payload.
        const auto opcodeValue{reader.readByte()};
        if (!opcodeValue.has_value()) {
            sendAll(clientFd, frameResponse(buildErrorResponse("missing opcode")));
            break;
        }
        const auto opcode{static_cast<RequestOpcode>(opcodeValue.value())};

        std::vector<std::uint8_t> response{handleRequest(opcode, reader, store)};

        std::cout << "request opcode: " << opcodeName(opcode)
                  << " (" << static_cast<int>(opcodeValue.value()) << ")\n";

        if (!sendAll(clientFd, frameResponse(response))) {
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

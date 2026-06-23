#include "RemoteListStub.h"
#include "SocketUtils.h"
#include "types.h"

#include <stdexcept>

RemoteListStub::RemoteListStub(const std::string& host, int port) {
    socket_fd_ = connectToServer(host, port);
    if (socket_fd_ < 0) {
        throw std::runtime_error("Failed to connect to server at " + host + ":" + std::to_string(port));
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(socket_fd_)};
    if (!responseMessage.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Failed to receive client ID from server");
    }

    std::optional<BinaryResponse> response{parseResponseMessage(responseMessage.value())};
    if (!response.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Malformed client ID response from server");
    }

    std::optional<std::int32_t> clientId{parseClientIdResponse(response.value())};
    if (!clientId.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Server did not provide a valid client ID");
    }

    client_id_ = clientId.value();
}

RemoteListStub::~RemoteListStub() {
    closeSocket(socket_fd_);
    socket_fd_ = -1;
}

// Sends a request for the given opcode and arguments, by framing the request according to the
// application protocol.
std::optional<BinaryResponse> RemoteListStub::sendRequest(RequestOpcode opcode,
                                                          const std::vector<std::uint8_t>& arguments) {
    if (!isConnected() || client_id_ < 0) {
        return std::nullopt;
    }

    const std::int32_t messageId{next_message_id_++};
    const std::vector<std::uint8_t> request{frameRequest(client_id_, messageId, opcode, arguments)};
    if (!sendAll(socket_fd_, request)) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        client_id_ = -1;
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(socket_fd_)};
    if (!responseMessage.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        client_id_ = -1;
        return std::nullopt;
    }

    return parseResponseMessage(responseMessage.value());
}

bool RemoteListStub::isConnected() const {
    return socket_fd_ >= 0;
}

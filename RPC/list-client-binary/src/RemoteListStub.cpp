#include "RemoteListStub.h"
#include "SocketUtils.h"
#include "types.h"

#include <stdexcept>

RemoteListStub::RemoteListStub(const std::string& host, int port) {
    socket_fd_ = connectToServer(host, port);
    if (socket_fd_ < 0) {
        throw std::runtime_error("Failed to connect to server at " + host + ":" + std::to_string(port));
    }
}

RemoteListStub::~RemoteListStub() {
    closeSocket(socket_fd_);
    socket_fd_ = -1;
}

// Sends a request for the given opcode and arguments, by framing the request according to the
// application protocol.
std::optional<BinaryResponse> RemoteListStub::sendRequest(RequestOpcode opcode,
                                                          const std::vector<std::uint8_t>& arguments) {
    if (!isConnected()) {
        return std::nullopt;
    }

    const std::vector<std::uint8_t> request{frameRequest(opcode, arguments)};
    if (!sendAll(socket_fd_, request)) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(socket_fd_)};
    if (!responseMessage.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        return std::nullopt;
    }

    return parseResponseMessage(responseMessage.value());
}

bool RemoteListStub::isConnected() const {
    return socket_fd_ >= 0;
}

#include "remote_list_stub.h"
#include "socket_utils.h"
#include <iostream>
#include <stdexcept>

RemoteListStub::RemoteListStub(const std::string& host, int port) {
    socket_fd_ = connectToServer(host, port);
    if (socket_fd_ < 0) {
        throw std::runtime_error("Failed to connect to server at " + host + ":" + std::to_string(port));
    }

    // Read the initial greeting from the server
    auto greeting = readMessage(socket_fd_);
    if (!greeting.has_value()) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        throw std::runtime_error("Failed to receive greeting from server");
    }
}

RemoteListStub::~RemoteListStub() {
    closeSocket(socket_fd_);
    socket_fd_ = -1;
}

std::optional<std::string> RemoteListStub::sendCommand(const std::string& command) {
    if (!isConnected()) {
        return std::nullopt;
    }

    // Send command with newline terminator
    if (!sendAll(socket_fd_, command + "\n")) {
        closeSocket(socket_fd_);
        socket_fd_ = -1;
        return std::nullopt;
    }

    // Read response
    return readMessage(socket_fd_);
}

bool RemoteListStub::isConnected() const {
    return socket_fd_ >= 0;
}

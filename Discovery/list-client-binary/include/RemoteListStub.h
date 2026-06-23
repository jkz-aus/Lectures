#ifndef REMOTE_LIST_STUB_H
#define REMOTE_LIST_STUB_H

#include "types.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// A helper class for sending requests and getting back responses from a server.
class RemoteListStub {
public:
    RemoteListStub(const std::string& host, int port);
    ~RemoteListStub();

    RemoteListStub(const RemoteListStub&) = delete;
    RemoteListStub& operator=(const RemoteListStub&) = delete;

    std::optional<BinaryResponse> sendRequest(RequestOpcode opcode,
                                              const std::vector<std::uint8_t>& arguments);
    bool isConnected() const;
    int getSocket() const { return socket_fd_; }

private:
    int socket_fd_;
    std::int32_t client_id_{-1};
    std::int32_t next_message_id_{1};
};

#endif // REMOTE_LIST_STUB_H

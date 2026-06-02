#ifndef REMOTE_LIST_STUB_H
#define REMOTE_LIST_STUB_H

#include <optional>
#include <string>

/**
 * @file remote_list_stub.h
 * @brief Low-level RPC stub for list operations
 * 
 * Handles the protocol details: formatting commands, sending them,
 * and receiving responses. This is the "stub" layer in RPC terminology.
 */

class RemoteListStub {
public:
    /**
     * Constructs and connects the RPC stub to the server.
     * 
     * @param host Server hostname or IP
     * @param port Server port
     * @throws std::runtime_error if connection fails
     */
    RemoteListStub(const std::string& host, int port);

    /**
     * Destructor - closes the socket connection.
     */
    ~RemoteListStub();

    // Disable copy operations
    RemoteListStub(const RemoteListStub&) = delete;
    RemoteListStub& operator=(const RemoteListStub&) = delete;

    /**
     * Sends a command to the server and receives the response.
     * 
     * @param command The command string (e.g., "PUSH hello")
     * @return Response from server, or std::nullopt if communication fails
     */
    std::optional<std::string> sendCommand(const std::string& command);

    /**
     * Checks if the connection is still active.
     * 
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * Gets the socket file descriptor.
     * 
     * @return Socket fd, or -1 if not connected
     */
    int getSocket() const { return socket_fd_; }

private:
    int socket_fd_;
};

#endif // REMOTE_LIST_STUB_H

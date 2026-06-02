#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <optional>
#include <string>

/**
 * @file socket_utils.h
 * @brief Low-level socket communication utilities
 * 
 * Provides basic socket operations for TCP communication.
 */

/**
 * Safely closes a socket file descriptor.
 * @param fd Socket file descriptor to close
 */
void closeSocket(int fd);

/**
 * Connects to a remote server.
 * @param host Server hostname or IP address
 * @param port Server port number
 * @return Socket file descriptor on success, or -1 on failure
 */
int connectToServer(const std::string& host, int port);

/**
 * Sends all data in a message through the socket.
 * Handles partial sends until all data is transmitted.
 * @param fd Socket file descriptor
 * @param message Message to send
 * @return true if entire message was sent, false otherwise
 */
bool sendAll(int fd, const std::string& message);

/**
 * Reads a single message (up to newline) from the socket.
 * @param fd Socket file descriptor
 * @return Message string without newline, or std::nullopt on error/disconnect
 */
std::optional<std::string> readMessage(int fd);

#endif // SOCKET_UTILS_H

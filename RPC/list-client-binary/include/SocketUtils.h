#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

void closeSocket(int fd);
int connectToServer(const std::string& host, int port);
bool sendAll(int fd, const std::vector<std::uint8_t>& message);
std::optional<std::vector<std::uint8_t>> readMessage(int fd);

#endif // SOCKET_UTILS_H

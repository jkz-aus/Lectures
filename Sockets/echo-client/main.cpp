#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
    constexpr int PORT{9090};
    constexpr int BUFFER_SIZE{1024};

    void close_socket(int fd) {
        if (fd >= 0) {
            close(fd);
        }
    }

    void fail(const std::string& message) {
        throw std::runtime_error{message + ": " + std::strerror(errno)};
    }
}

int main() {
    try {
        int client_fd{socket(AF_INET, SOCK_STREAM, 0)};
        if (client_fd < 0) {
            fail("socket failed");
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
            close_socket(client_fd);
            fail("invalid server address");
        }

        if (connect(
                client_fd,
                reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)
            ) < 0) {
            close_socket(client_fd);
            fail("connect failed");
        }

        std::cout << "Connected to server on port " << PORT << ".\n";
        std::cout << "Type messages. Type Ctrl+D to quit.\n\n";

        std::string line{};

        while (std::getline(std::cin, line)) {
            line += '\n';

            ssize_t bytes_sent{send(client_fd, line.c_str(), line.size(), 0)};

            if (bytes_sent < 0) {
                std::cerr << "send failed: " << std::strerror(errno) << "\n";
                break;
            }

            char buffer[BUFFER_SIZE]{};

            ssize_t bytes_received{recv(client_fd, buffer, BUFFER_SIZE - 1, 0)};

            if (bytes_received < 0) {
                std::cerr << "recv failed: " << std::strerror(errno) << "\n";
                break;
            }

            if (bytes_received == 0) {
                std::cout << "Server disconnected.\n";
                break;
            }

            buffer[bytes_received] = '\0';

            std::cout << "Server echoed: " << buffer;
        }

        close_socket(client_fd);
    }
    catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
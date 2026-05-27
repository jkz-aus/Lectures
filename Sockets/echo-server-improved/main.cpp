#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
// This echo server handles messages that don't fit in just one buffer,
// by reading one character at a time until a \n is reached.
namespace {
    constexpr int PORT{9090};
    constexpr int BACKLOG{5};
    constexpr int BUFFER_SIZE{16};

    void close_socket(int fd) {
        if (fd >= 0) {
            close(fd);
        }
    }

    void fail(const std::string& message) {
        throw std::runtime_error{message + ": " + std::strerror(errno)};
    }

    // Read one character at a time from the socket, until \n is reached, and return
    // the resulting message.
    std::string readMessage(int client_fd) {
       std::string message{};
        char c{};

        while (true) {
            ssize_t bytesReceived{recv(client_fd, &c, 1, 0)};
            if (bytesReceived < 0) {
                std::cerr << "recv failed: " << std::strerror(errno) << "\n";
                return "";
            }
            if (bytesReceived == 0) {
                return "";
            }

            if (c == '\n') {
                break;
            }

            message += c;
        }
        return message;
    }

}

int main() {
    try {
        // Initialize an Internet (AF_INET) socket using a reliable TCP connection (SOCK_STREAM).
        int server_fd{socket(AF_INET, SOCK_STREAM, 0)};
        if (server_fd < 0) {
            fail("socket failed");
        }

        int reuse{1};
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            close_socket(server_fd);
            fail("setsockopt failed");
        }

        // Define the address at which the socket will accept connection requests.
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET; // Internet connections only.
        server_addr.sin_addr.s_addr = INADDR_ANY; // Any networking interface on the computer (Wi-Fi, ethernet, etc.)
        server_addr.sin_port = htons(PORT); // Convert the port number from local-machine endianness to Network (big) endianness.

        // Bind our socket to the desired address.
        if (bind(server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            close_socket(server_fd);
            fail("bind failed");
        }

        // Wait for a connection request.
        if (listen(server_fd, BACKLOG) < 0) {
            close_socket(server_fd);
            fail("listen failed");
        }

        std::cout << "Server listening on port " << PORT << "...\n";

        // Initialize a second socket to use when responding to a client.
        sockaddr_in client_addr{};
        socklen_t client_len{sizeof(client_addr)};

        // Accept the incoming request, and initialize the response socket.
        // This will block the process until a client connects.
        int client_fd{accept(
            server_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        )};

        if (client_fd < 0) {
            close_socket(server_fd);
            fail("accept failed");
        }

        char client_ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        std::cout << "Client connected from "
                  << client_ip << ":"
                  << ntohs(client_addr.sin_port)
                  << "\n";

        char buffer[BUFFER_SIZE]{};

        while (true) {
            // Use readMessage to get the next message from the client.
            std::string message{readMessage(client_fd)};
            if (message.empty()) {
                break;
            }

            // Echo the message to stdout.
            std::cout << "Received: " << message << std::endl;

            // Respond to the client with the same message.
            ssize_t bytes_sent{send(client_fd, message.c_str(), message.size(), 0)};

            if (bytes_sent < 0) {
                std::cerr << "send failed: " << std::strerror(errno) << "\n";
                break;
            }
        }

        close_socket(client_fd);
        close_socket(server_fd);
    }
    catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
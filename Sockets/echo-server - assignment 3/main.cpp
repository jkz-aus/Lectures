#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <format>
#include <thread>
#include <mutex>

namespace {
    constexpr int PORT{9090};
    constexpr int BACKLOG{5};
    constexpr int BUFFER_SIZE{1024};
    constexpr int MAX_CLIENTS{3};

    void close_socket(int fd) {
        if (fd >= 0) {
            close(fd);
        }
    }

    void fail(const std::string& message) {
        throw std::runtime_error{message + ": " + std::strerror(errno)};
    }

void handleClient(int client_fd, int& client_count, std::mutex& client_mutex) {char buffer[BUFFER_SIZE]{};
        {
            //lock
            std::lock_guard<std::mutex> lock{client_mutex};
            //increment
            client_count++;
        }
        while (true) {
            // Receive up to BUFFER_SIZE-1 bytes from the client.
            ssize_t bytes_received{recv(client_fd, buffer, BUFFER_SIZE - 1, 0)};

            if (bytes_received < 0) {
                std::cerr << "recv failed: " << std::strerror(errno) << "\n";
                break;
            }

            if (bytes_received == 0) {
                std::cout << "Client disconnected.\n";
                break;
            }

            // We know we got a successful message from the client.

            buffer[bytes_received] = '\0';

            std::string message{buffer};

            // Echo the message to stdout.
            std::cout << "Received: " << message;

            std::string output;

            //checks the first 4 elements to ECHO as the condition
            if (message.substr (0,4) == "ECHO") {
                output = message.substr(5) + "\n";
            }

            //checks first 4 elements to TIME as the condition
            else if (message.substr (0,4) == "TIME") {
                auto now = std::chrono::system_clock::now(); //gets current system time
                std::string sys_time = std::format("{:%T}", now); //formats to ISO 8601 hh:mm:ss
                output = sys_time.substr(0,8) + "\n";
            }

            //checks if message starts with HELP
            else if (message.substr (0,4) == "HELP") {
                output = "Available operations: ECHO, TIME, HELP, QUIT\n";
            }

            else if (message.substr (0,7) == "CLIENTS") {
                std::lock_guard<std::mutex> lock{client_mutex};
                output = "CLIENTS: " + std::to_string(client_count) + "\n";
            }

            else if (message.substr(0,4) == "QUIT") {
                output = "GOODBYE\n"; //outputs GOODBYE
            }

            else {
                output = "ERROR unknown command\n";
            }

            // Respond to the client with the same message.
            ssize_t bytes_sent{send(client_fd, output.c_str(), output.size(), 0)};

            if (bytes_sent < 0) {
                std::cerr << "send failed: " << std::strerror(errno) << "\n";
                break;
            }


            if (message.substr (0,4) == "QUIT") {
                break;
            }
        }

        {
            //lock
            std::lock_guard<std::mutex> lock{client_mutex};
            //decrement
            client_count--;
        }

        close_socket(client_fd);
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

        int client_count = 0;
        std::mutex client_mutex{};

        while (true) {
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

            {
                std::lock_guard<std::mutex> lock{client_mutex};
                if (client_count >= MAX_CLIENTS) {
                    std::string error{"ERROR too many clients\n"};
                    send(client_fd, error.c_str(), error.size(), 0);
                    close_socket(client_fd);
                    continue;
                }
            }

            char client_ip[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

            std::cout << "Client connected from "
                      << client_ip << ":"
                      << ntohs(client_addr.sin_port)
                      << "\n";

            //handleClient(client_fd);
            std::thread clientThread{handleClient, client_fd, std::ref(client_count), std::ref(client_mutex)};
            clientThread.detach();
        }

        close_socket(server_fd);
    }

    catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

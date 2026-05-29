#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* This key-value server uses one thread per client that connects on the socket. A mutual exclusion lock
 * ensures that only one thread accesses the store at a time.
 */



struct Command {
    std::string name{};
    std::vector<std::string> args{};
};


void closeSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

/* The send() function attempts to send all the bytes you give it to the destination socket. It returns the number
 * of bytes *actually* sent. Why would that number be different than the size of the buffer you sent? Because there are
 * many many concerns when sending TCP segments -> IP packets that might limit how many byte can be sent in a single
 * call. We can't predict that limit, so send() will do its best, and return the number of bytes actually sent.
 * If the number sent is LESS THAN the number we wanted to send, then we need to loop a second send() call, skipping
 * past all the bytes that were sent. We keep calling send() until the sum of all the bytes sent equals the length of
 * the message.
 * sendAll() encapsulates that behavior, to send the entirety of a string message to a given socket.
 */
bool sendAll(int fd, const std::string& message) {
    // Get a pointer to the beginning of the message string.
    const char* data{message.c_str()};
    // Track how many bytes we've sent, and how many we need to send total.
    std::size_t totalSent{0};
    std::size_t totalSize{message.size()};

    while (totalSent < totalSize) {
        // Skip past the bytes we've already sent.
        const char* dataOffset {data + totalSent};
        // dataOffset points to the middle of the string, after skipping "totalSent" characters from the beginning.

        // Send the next set of bytes.
        ssize_t bytes_sent{send(fd, dataOffset, totalSize - totalSent, 0)};

        if (bytes_sent < 0) {
            return false;
        }

        if (bytes_sent == 0) {
            return false;
        }

        totalSent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

// std::optional is C++'s way of handling optional/nullable values. This function might
// return a string if it's successful, or it might return "null" indicating failure.
std::optional<std::string> readMessage(int fd) {
    std::string message{};
    char ch{};

    while (true) {
        ssize_t bytesReceived{recv(fd, &ch, 1, 0)};

        if (bytesReceived <= 0) {
            return std::nullopt; // return "null".
        }

        if (ch == '\n') {
            break;
        }
        message += ch;
    }
    return message;
}

// Parse the command and its arguments from a message.
Command parseCommand(const std::string& message) {
    std::istringstream input{message};

    Command command{};
    input >> command.name;

    std::string arg{};
    while (input >> arg) {
        command.args.push_back(arg);
    }

    return command;
}

// Apply a Command to the current store.
std::string handleCommand(const Command& command,
                          std::unordered_map<std::string, std::string>& store) {
    if (command.name.empty()) {
        return "ERROR empty command\n";
    }

    if (command.name == "PUT") {
        if (command.args.size() != 2) {
            return "ERROR PUT requires key and value\n";
        }

        const std::string& key{command.args[0]};
        const std::string& value{command.args[1]};

        store[key] = value;
        return "OK\n";
    }

    if (command.name == "GET") {
        if (command.args.size() != 1) {
            return "ERROR GET requires key\n";
        }
        const std::string& key{command.args[0]};
        auto it{store.find(key)};

        if (it == store.end()) {
            return "NOT_FOUND\n";
        }

        return "VALUE " + it->second + "\n";

    }

    if (command.name == "DELETE") {
        if (command.args.size() != 1) {
            return "ERROR DELETE requires key\n";
        }

        const std::string& key{command.args[0]};
        std::size_t removed{store.erase(key)};
        if (removed == 0) {
            return "NOT_FOUND\n";
        }

        return "OK\n";
    }

    if (command.name == "COUNT") {
        if (!command.args.empty()) {
            return "ERROR COUNT takes no arguments\n";
        }

        std::size_t size{store.size()};
        return "COUNT " + std::to_string(size) + "\n";

    }

    if (command.name == "QUIT") {
        if (!command.args.empty()) {
            return "ERROR QUIT takes no arguments\n";
        }

        return "BYE\n";
    }

    return "ERROR unknown command\n";
}

void handleClient(int client_fd, std::unordered_map<std::string, std::string>& store) {
    sendAll(client_fd, "OK connected\n");

    while (true) {
        std::optional<std::string> message{readMessage(client_fd)};
        // Since readMessage might return null, we have to test the return value
        // before using it like a string.

        if (!message.has_value()) {
            // This occurs if null was returned.
            std::cout << "Client disconnected\n";
            break;
        }

        // We can now use .value() to get the actual string that was returned.
        std::string line{message.value()};

        // Parse the message into a command.
        Command command{parseCommand(line)};

        // Handle the command and get the response.
        std::string response{handleCommand(command, store)};

        std::cout << "request:  " << line << '\n';
        std::cout << "response: " << response;

        // Send the response to the client.
        if (!sendAll(client_fd, response)) {
            std::cout << "Failed to send response\n";
            break;
        }

        if (command.name == "QUIT" && response == "BYE\n") {
            break;
        }
    }

    closeSocket(client_fd);
}

int createServerSocket(int port) {
    int server_fd{socket(AF_INET, SOCK_STREAM, 0)};

    if (server_fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        std::exit(1);
    }

    int yes{1};
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << '\n';
        closeSocket(server_fd);
        std::exit(1);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd,
             reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << '\n';
        closeSocket(server_fd);
        std::exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        closeSocket(server_fd);
        std::exit(1);
    }

    return server_fd;
}

int main(int argc, char* argv[]) {
    constexpr int PORT{9090};

    int server_fd{createServerSocket(PORT)};
    std::unordered_map<std::string, std::string> store{};

    std::cout << "Key-value server listening on port " << PORT << '\n';

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len{sizeof(client_addr)};

        int client_fd{
            accept(server_fd,
                   reinterpret_cast<sockaddr*>(&client_addr),
                   &client_len)
        };

        if (client_fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        char client_ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        std::cout << "Accepted client from "
                  << client_ip
                  << ':'
                  << ntohs(client_addr.sin_port)
                  << '\n';

        std::thread clientThread{handleClient, client_fd, std::ref(store)};
        clientThread.detach();
    }

    closeSocket(server_fd);
    return 0;
}

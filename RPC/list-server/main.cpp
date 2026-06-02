#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

/* This list server uses one thread per client that connects on the socket. A mutual exclusion lock
 * ensures that only one thread accesses the store at a time.
 */

struct Command {
    std::string name{};
    std::vector<std::string> args{};
};

/* The vector is a critical resource: only one thread can access it at a time, or else race conditions occur
 * and the list can be corrupted. In addition to passing the list to handleClient(), we also need to pass a mutual
 * exclusion lock, so we wrap the two together in a single SharedStore object.
 */
struct SharedStore {
    std::vector<std::string> values{};
    std::mutex mutex{};
};

void closeSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

bool sendAll(int fd, const std::string& message) {
    const char* data{message.c_str()};
    std::size_t totalSent{0};
    std::size_t totalSize{message.size()};

    while (totalSent < totalSize) {
        const char* dataOffset{data + totalSent};
        ssize_t bytesSent{send(fd, dataOffset, totalSize - totalSent, 0)};

        if (bytesSent <= 0) {
            return false;
        }

        totalSent += static_cast<std::size_t>(bytesSent);
    }

    return true;
}

std::optional<std::string> readMessage(int fd) {
    std::string message{};
    char ch{};

    while (true) {
        ssize_t bytesReceived{recv(fd, &ch, 1, 0)};

        if (bytesReceived <= 0) {
            return std::nullopt;
        }

        if (ch == '\n') {
            break;
        }

        message += ch;
    }

    return message;
}



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

std::optional<std::size_t> parseIndex(const std::string& text) {
    try {
        std::size_t position{};
        unsigned long long value{std::stoull(text, &position)};

        if (position != text.size()) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(value);
    } catch (...) {
        return std::nullopt;
    }
}

std::string handleCommand(const Command& command, SharedStore& store) {
    if (command.name.empty()) {
        return "ERROR empty command\n";
    }

    if (command.name == "PUSH") {
        if (command.args.size() != 1) {
            return "ERROR PUSH requires value\n";
        }

        {
            std::lock_guard<std::mutex> lock{store.mutex};
            store.values.push_back(command.args[0]);
        }

        return "OK\n";
    }

    if (command.name == "POP") {
        if (!command.args.empty()) {
            return "ERROR POP takes no arguments\n";
        }

        std::string value{};

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (store.values.empty()) {
                return "ERROR list is empty\n";
            }

            value = store.values.back();
            store.values.pop_back();
        }

        return "VALUE " + value + "\n";
    }

    if (command.name == "INSERT") {
        if (command.args.size() != 2) {
            return "ERROR INSERT requires index and value\n";
        }

        std::optional<std::size_t> index{parseIndex(command.args[0])};
        if (!index.has_value()) {
            return "ERROR invalid index\n";
        }

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (index.value() > store.values.size()) {
                return "ERROR invalid index\n";
            }

            store.values.insert(store.values.begin() + static_cast<std::ptrdiff_t>(index.value()),
                                command.args[1]);
        }

        return "OK\n";
    }

    if (command.name == "REMOVE") {
        if (command.args.size() != 1) {
            return "ERROR REMOVE requires index\n";
        }

        std::optional<std::size_t> index{parseIndex(command.args[0])};
        if (!index.has_value()) {
            return "ERROR invalid index\n";
        }

        std::string value{};

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (index.value() >= store.values.size()) {
                return "ERROR invalid index\n";
            }

            value = store.values[index.value()];
            store.values.erase(store.values.begin() + static_cast<std::ptrdiff_t>(index.value()));
        }

        return "VALUE " + value + "\n";
    }

    if (command.name == "COUNT") {
        if (!command.args.empty()) {
            return "ERROR COUNT takes no arguments\n";
        }

        std::size_t size{};

        {
            std::lock_guard<std::mutex> lock{store.mutex};
            size = store.values.size();
        }

        return "COUNT " + std::to_string(size) + "\n";
    }

    if (command.name == "GET") {
        if (command.args.size() != 1) {
            return "ERROR GET requires index\n";
        }

        std::optional<std::size_t> index{parseIndex(command.args[0])};
        if (!index.has_value()) {
            return "ERROR invalid index\n";
        }

        std::string value{};

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (index.value() >= store.values.size()) {
                return "ERROR invalid index\n";
            }

            value = store.values[index.value()];
        }

        return "VALUE " + value + "\n";
    }

    if (command.name == "SET") {
        if (command.args.size() != 2) {
            return "ERROR SET requires index and value\n";
        }

        std::optional<std::size_t> index{parseIndex(command.args[0])};
        if (!index.has_value()) {
            return "ERROR invalid index\n";
        }

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (index.value() >= store.values.size()) {
                return "ERROR invalid index\n";
            }

            store.values[index.value()] = command.args[1];
        }

        return "OK\n";
    }

    if (command.name == "SWAP") {
        if (command.args.size() != 2) {
            return "ERROR SWAP requires two indices\n";
        }

        std::optional<std::size_t> firstIndex{parseIndex(command.args[0])};
        std::optional<std::size_t> secondIndex{parseIndex(command.args[1])};
        if (!firstIndex.has_value() || !secondIndex.has_value()) {
            return "ERROR invalid index\n";
        }

        {
            std::lock_guard<std::mutex> lock{store.mutex};

            if (firstIndex.value() >= store.values.size() || secondIndex.value() >= store.values.size()) {
                return "ERROR invalid index\n";
            }

            std::swap(store.values[firstIndex.value()], store.values[secondIndex.value()]);
        }

        return "OK\n";
    }

    if (command.name == "CLEAR") {
        if (!command.args.empty()) {
            return "ERROR CLEAR takes no arguments\n";
        }

        {
            std::lock_guard<std::mutex> lock{store.mutex};
            store.values.clear();
        }

        return "OK\n";
    }

    if (command.name == "QUIT") {
        if (!command.args.empty()) {
            return "ERROR QUIT takes no arguments\n";
        }

        return "BYE\n";
    }

    return "ERROR unknown command\n";
}

void handleClient(int clientFd, SharedStore& store) {
    sendAll(clientFd, "OK connected\n");

    while (true) {
        std::optional<std::string> message{readMessage(clientFd)};

        if (!message.has_value()) {
            std::cout << "Client disconnected\n";
            break;
        }

        std::string line{message.value()};
        Command command{parseCommand(line)};
        std::string response{handleCommand(command, store)};

        std::cout << "request:  " << line << '\n';
        std::cout << "response: " << response;

        if (!sendAll(clientFd, response)) {
            std::cout << "Failed to send response\n";
            break;
        }

        if (command.name == "QUIT" && response == "BYE\n") {
            break;
        }
    }

    closeSocket(clientFd);
}

int createServerSocket(int port) {
    int serverFd{socket(AF_INET, SOCK_STREAM, 0)};

    if (serverFd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        std::exit(1);
    }

    int yes{1};
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        closeSocket(serverFd);
        std::exit(1);
    }

    return serverFd;
}

int main() {
    constexpr int PORT{9090};

    int serverFd{createServerSocket(PORT)};
    SharedStore store{};

    std::cout << "List server listening on port " << PORT << '\n';

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen{sizeof(clientAddr)};

        int clientFd{
            accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen)
        };

        if (clientFd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        char clientIp[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

        std::cout << "Accepted client from "
                  << clientIp
                  << ':'
                  << ntohs(clientAddr.sin_port)
                  << '\n';

        std::thread clientThread{handleClient, clientFd, std::ref(store)};
        clientThread.detach();
    }

    closeSocket(serverFd);
    return 0;
}
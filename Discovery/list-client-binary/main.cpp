#include "RemoteList.h"
#include "MessageReader.h"
#include "SocketUtils.h"
#include "types.h"

#include <charconv>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

enum class NameServerRequestOpcode : std::uint8_t {
    Resolve = 0x04
};

enum class NameServerResponseOpcode : std::uint8_t {
    Provider = 0x41,
    ClientId = 0x44,
    Error = 0x7F
};

struct ResolvedProvider {
    std::string identifier{};
    std::string host{};
    int port{};
};

bool parsePort(const char* text, int& port) {
    if (text == nullptr) {
        return false;
    }

    int parsedPort{};
    const char* end{text + std::strlen(text)};
    const auto [ptr, error]{std::from_chars(text, end, parsedPort)};
    if (error != std::errc{} || ptr != end || parsedPort <= 0 || parsedPort > 65535) {
        return false;
    }

    port = parsedPort;
    return true;
}

std::optional<ResolvedProvider> resolveListService(const std::string& nameServerHost, int nameServerPort) {
    constexpr const char* SERVICE_NAME{"list-service"};

    int fd{connectToServer(nameServerHost, nameServerPort)};
    if (fd < 0) {
        std::cerr << "Failed to connect to name server at "
                  << nameServerHost
                  << ':'
                  << nameServerPort
                  << '\n';
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> handshakeMessage{readMessage(fd)};
    if (!handshakeMessage.has_value()) {
        std::cerr << "Failed to receive name server handshake\n";
        closeSocket(fd);
        return std::nullopt;
    }

    std::optional<BinaryResponse> handshake{parseResponseMessage(handshakeMessage.value())};
    if (!handshake.has_value() || handshake->opcode != ResponseOpcode::ClientId) {
        std::cerr << "Invalid name server handshake response\n";
        closeSocket(fd);
        return std::nullopt;
    }

    std::optional<std::int32_t> clientId{parseClientIdResponse(handshake.value())};
    if (!clientId.has_value()) {
        std::cerr << "Malformed client ID from name server\n";
        closeSocket(fd);
        return std::nullopt;
    }

    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, SERVICE_NAME)) {
        std::cerr << "Failed to encode resolve request\n";
        closeSocket(fd);
        return std::nullopt;
    }

    const std::vector<std::uint8_t> request{
        frameRequest(clientId.value(),
                     1,
                     static_cast<RequestOpcode>(NameServerRequestOpcode::Resolve),
                     arguments)
    };
    if (!sendAll(fd, request)) {
        std::cerr << "Failed to send resolve request to name server\n";
        closeSocket(fd);
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> responseMessage{readMessage(fd)};
    closeSocket(fd);
    if (!responseMessage.has_value()) {
        std::cerr << "Failed to receive resolve response from name server\n";
        return std::nullopt;
    }

    std::optional<BinaryResponse> response{parseResponseMessage(responseMessage.value())};
    if (!response.has_value()) {
        std::cerr << "Malformed resolve response from name server\n";
        return std::nullopt;
    }

    if (response->opcode == ResponseOpcode::Error) {
        std::cerr << "Name server resolve failed: " << getErrorMessage(response.value()) << '\n';
        return std::nullopt;
    }

    if (static_cast<std::uint8_t>(response->opcode) != static_cast<std::uint8_t>(NameServerResponseOpcode::Provider)) {
        std::cerr << "Unexpected resolve response from name server\n";
        return std::nullopt;
    }

    MessageReader reader{response->payload};
    std::optional<std::string> identifier{reader.readString()};
    std::optional<std::string> host{reader.readString()};
    std::optional<std::int32_t> port{reader.readInt32()};
    if (!identifier.has_value() || !host.has_value() || !port.has_value()
        || port.value() <= 0 || port.value() > 65535 || !reader.isAtEnd()) {
        std::cerr << "Malformed provider response from name server\n";
        return std::nullopt;
    }

    return ResolvedProvider{identifier.value(), host.value(), port.value()};
}

}

void printMenu() {
    std::cout << "\n" << std::string(60, '=') << '\n';
    std::cout << "  Remote List Menu\n";
    std::cout << std::string(60, '=') << '\n';
    std::cout << "  1) Add strings to list (PUSH)\n";
    std::cout << "  2) Sort list using insertion sort\n";
    std::cout << "  0) Exit\n";
    std::cout << std::string(60, '=') << '\n';
}

void displayList(RemoteList& list) {
    auto cnt = list.count();
    if (!cnt.has_value()) {
        std::cout << "  [Error getting list size]\n";
        return;
    }

    if (cnt.value() == 0) {
        std::cout << "  [Empty list]\n";
        return;
    }

    for (std::size_t i = 0; i < cnt.value(); ++i) {
        auto val = list.get(i);
        std::cout << "  [" << i << "] " << (val.has_value() ? val.value() : "[ERROR]") << '\n';
    }
}

void optionPush(RemoteList& list) {
    std::cout << "\n--- Option 1: Add Strings to List ---\n";
    std::cout << "Enter strings to add to the list (empty line to finish):\n";

    std::string line;
    int count = 0;

    while (true) {
        std::cout << "  > ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) {
            break;
        }

        if (list.push(line)) {
            count++;
            std::cout << "    Added: " << line << '\n';
        } else {
            std::cout << "    Failed to push: " << line << '\n';
        }
    }

    std::cout << "\nAdded " << count << " items to the list.\n";
    std::cout << "\nCurrent list contents:\n";
    displayList(list);
}

void optionSortInsertionSort(RemoteList& list) {
    std::cout << "\n--- Option 2: Sort List (Insertion Sort) ---\n";

    auto cnt = list.count();
    if (!cnt.has_value()) {
        std::cout << "Error: Could not get list size\n";
        return;
    }

    std::size_t n = cnt.value();
    if (n == 0) {
        std::cout << "List is empty, nothing to sort.\n";
        return;
    }

    std::cout << "Sorting " << n << " items using insertion sort...\n";

    for (std::size_t i = 1; i < n; ++i) {
        auto key = list.get(i);
        if (!key.has_value()) {
            std::cout << "Error reading element at index " << i << '\n';
            return;
        }

        std::string keyValue = key.value();
        std::size_t j = i;

        while (j > 0) {
            auto prev = list.get(j - 1);
            if (!prev.has_value()) {
                std::cout << "Error reading element at index " << (j - 1) << '\n';
                return;
            }

            if (prev.value() > keyValue) {
                if (!list.set(j, prev.value())) {
                    std::cout << "Error setting element at index " << j << '\n';
                    return;
                }
                j--;
            } else {
                break;
            }
        }

        if (!list.set(j, keyValue)) {
            std::cout << "Error setting element at index " << j << '\n';
            return;
        }
    }

    std::cout << "Sorting complete!\n";
    std::cout << "\nSorted list contents:\n";
    displayList(list);
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <name-server-host> <name-server-port>\n";
            return 1;
        }

        int nameServerPort{};
        if (!parsePort(argv[2], nameServerPort)) {
            std::cerr << "Invalid name server port: " << argv[2] << '\n';
            return 1;
        }

        std::cout << "\n" << std::string(60, '=') << '\n';
        std::cout << "  Remote List RPC Client\n";
        std::cout << std::string(60, '=') << '\n';

        std::optional<ResolvedProvider> provider{resolveListService(argv[1], nameServerPort)};
        if (!provider.has_value()) {
            return 1;
        }

        std::cout << "\nResolved list-service provider "
                  << provider->identifier
                  << " at "
                  << provider->host
                  << ':'
                  << provider->port
                  << '\n';
        std::cout << "Connecting to resolved list server...\n";

        RemoteList list(provider->host, provider->port);
        std::cout << "Connected!\n";

        int choice = -1;
        while (choice != 0) {
            printMenu();
            std::cout << "Enter choice (0-2): ";
            std::cin >> choice;
            std::cin.ignore();

            switch (choice) {
                case 0:
                    std::cout << "\nGoodbye!\n\n";
                    break;
                case 1:
                    optionPush(list);
                    break;
                case 2:
                    optionSortInsertionSort(list);
                    break;
                default:
                    std::cout << "\nInvalid choice. Please enter 0, 1, or 2.\n";
                    break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

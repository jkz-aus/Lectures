#include "RemoteKeyValueStore.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

void printMenu() {
    std::cout << "\n" << std::string(60, '=') << '\n';
    std::cout << "  Remote List Menu\n";
    std::cout << std::string(60, '=') << '\n';
    std::cout << "  1) Add pairs to the store\n";
    std::cout << "  2) Print store values\n";
    std::cout << "  0) Exit\n";
    std::cout << std::string(60, '=') << '\n';
}

void printKeys(RemoteKeyValueStore& store) {
    auto keys = store.keys();
    if (!keys.has_value()) {
        return;
    }

    for (const auto& key : keys.value()) {
        std::cout << key << "\n";
    }
}

void optionAddPairs(RemoteKeyValueStore& store) {
    std::string line;

    while (std::getline(std::cin, line) && !line.empty()) {
        std::istringstream input{line};
        std::string key;
        std::string value;

        input >> key >> value;
        if (!key.empty() && !value.empty()) {
            store.put(key, value);
        }
    }
    printKeys(store);
}

void optionPrintValues(RemoteKeyValueStore& store) {
    auto keys = store.keys();
    if (!keys.has_value()) {
        return;
    }
    std::vector<std::string> values{};

    for (const auto& key : keys.value()) {
        auto value = store.get(key);
        if (value.has_value()) {
            values.push_back(value.value());
        }
    }
    std::sort(values.begin(), values.end());

    for (const auto& value : values) {
        std::cout << value << "\n";
    }
}

int main() {
    try {
        std::cout << "\n" << std::string(60, '=') << '\n';
        std::cout << "  Remote List RPC Client\n";
        std::cout << std::string(60, '=') << '\n';
        std::cout << "\nConnecting to server at 127.0.0.1:9090...\n";

        RemoteKeyValueStore store("127.0.0.1", 9090);
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
                    optionAddPairs(store);
                    break;
                case 2:
                    optionPrintValues(store);
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

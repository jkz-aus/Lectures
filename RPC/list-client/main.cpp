#include "remote_list.h"
#include <iostream>
#include <iomanip>
#include <string>

/**
 * Remote List RPC Client with Menu
 * 
 * Option 1: Add strings to the list via PUSH until empty line
 * Option 2: Sort the remote list using insertion sort
 */

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

        // Empty line signals end of input
        if (line.empty()) {
            break;
        }

        if (list.push(line)) {
            count++;
            std::cout << "    ✓ Added: " << line << '\n';
        } else {
            std::cout << "    ✗ Failed to push: " << line << '\n';
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
        std::cout << "✗ Error: Could not get list size\n";
        return;
    }

    std::size_t n = cnt.value();
    if (n == 0) {
        std::cout << "List is empty, nothing to sort.\n";
        return;
    }

    std::cout << "Sorting " << n << " items using insertion sort...\n";

    // Insertion sort algorithm
    for (std::size_t i = 1; i < n; ++i) {
        auto key = list.get(i);
        if (!key.has_value()) {
            std::cout << "✗ Error reading element at index " << i << '\n';
            return;
        }

        std::string keyValue = key.value();
        std::size_t j = i;

        // Shift elements greater than key to the right
        while (j > 0) {
            auto prev = list.get(j - 1);
            if (!prev.has_value()) {
                std::cout << "✗ Error reading element at index " << (j - 1) << '\n';
                return;
            }

            // Compare: if previous element is greater than key, shift right
            if (prev.value() > keyValue) {
                if (!list.set(j, prev.value())) {
                    std::cout << "✗ Error setting element at index " << j << '\n';
                    return;
                }
                j--;
            } else {
                break;
            }
        }

        // Insert key at its correct position
        if (!list.set(j, keyValue)) {
            std::cout << "✗ Error setting element at index " << j << '\n';
            return;
        }
    }

    std::cout << "✓ Sorting complete!\n";
    std::cout << "\nSorted list contents:\n";
    displayList(list);
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "\n" << std::string(60, '=') << '\n';
        std::cout << "  Remote List RPC Client\n";
        std::cout << std::string(60, '=') << '\n';
        std::cout << "\nConnecting to server at 127.0.0.1:9090...\n";

        RemoteList list("127.0.0.1", 9090);
        std::cout << "✓ Connected!\n";

        int choice = -1;
        while (choice != 0) {
            printMenu();
            std::cout << "Enter choice (0-2): ";
            std::cin >> choice;
            std::cin.ignore(); // Clear newline from input buffer

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
                    std::cout << "\n✗ Invalid choice. Please enter 0, 1, or 2.\n";
                    break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
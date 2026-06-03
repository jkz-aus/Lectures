#ifndef REMOTE_LIST_H
#define REMOTE_LIST_H

#include "remote_list_stub.h"
#include <optional>
#include <string>
#include <memory>

/**
 * @file remote_list.h
 * @brief High-level facade for remote list operations
 * 
 * Provides a clean C++ interface to a remote list stored on a server.
 * This is the "proxy" or "facade" layer that hides RPC protocol details.
 * 
 * Example usage:
 *   RemoteList list("127.0.0.1", 9090);
 *   list.push("hello");
 *   list.insert(0, "world");
 *   auto value = list.get(0);
 */

class RemoteList {
public:
    /**
     * Creates a RemoteList connected to the specified server.
     * 
     * @param host Server hostname or IP
     * @param port Server port
     * @throws std::runtime_error if connection fails
     */
    explicit RemoteList(const std::string& host = "127.0.0.1", int port = 9090);

    /**
     * Default destructor - closes server connection
     */
    ~RemoteList() = default;

    // Disable copy operations
    RemoteList(const RemoteList&) = delete;
    RemoteList& operator=(const RemoteList&) = delete;

    // Allow move operations
    RemoteList(RemoteList&&) = default;
    RemoteList& operator=(RemoteList&&) = default;

    /**
     * Adds a value to the end of the list.
     * 
     * @param value Value to push
     * @return true on success, false on error
     */
    bool push(const std::string& value);

    /**
     * Removes and returns the last value in the list.
     * 
     * @return Value that was popped, or std::nullopt on error
     */
    std::optional<std::string> pop();

    /**
     * Inserts a value at the specified index.
     * 
     * @param index Position to insert at
     * @param value Value to insert
     * @return true on success, false on error
     */
    bool insert(std::size_t index, const std::string& value);

    /**
     * Removes and returns the value at the specified index.
     * 
     * @param index Position to remove from
     * @return Value that was removed, or std::nullopt on error
     */
    std::optional<std::string> remove(std::size_t index);

    /**
     * Gets the number of elements in the list.
     * 
     * @return Number of elements, or std::nullopt on error
     */
    std::optional<std::size_t> count();

    /**
     * Gets the value at the specified index.
     * 
     * @param index Position to retrieve from
     * @return Value at that position, or std::nullopt on error
     */
    std::optional<std::string> get(std::size_t index);

    /**
     * Sets the value at the specified index.
     * 
     * @param index Position to set
     * @param value New value
     * @return true on success, false on error
     */
    bool set(std::size_t index, const std::string& value);

    /**
     * Swaps two elements in the list.
     * 
     * @param firstIndex First position
     * @param secondIndex Second position
     * @return true on success, false on error
     */
    bool swap(std::size_t firstIndex, std::size_t secondIndex);

    /**
     * Removes all elements from the list.
     * 
     * @return true on success, false on error
     */
    bool clear();

    /**
     * Checks if the connection is still active.
     * 
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

private:
    std::unique_ptr<RemoteListStub> stub_;

    /**
     * Helper to send a command and parse status response.
     * Used for operations that only return OK/ERROR.
     */
    bool sendStatusCommand(const std::string& command);

    /**
     * Helper to send a command and parse value response.
     * Used for operations that return a value.
     */
    std::optional<std::string> sendValueCommand(const std::string& command);
};

#endif // REMOTE_LIST_H

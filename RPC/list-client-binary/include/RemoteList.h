#pragma once
#include "RemoteListStub.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Abstracts a list of strings, which secretly communicates with a list-server-binary instance
// where the real list is stored.
class RemoteList {
public:
    explicit RemoteList(const std::string& host = "127.0.0.1", int port = 9090);
    ~RemoteList() = default;

    RemoteList(const RemoteList&) = delete;
    RemoteList& operator=(const RemoteList&) = delete;
    RemoteList(RemoteList&&) = default;
    RemoteList& operator=(RemoteList&&) = default;

    bool push(const std::string& value);
    std::optional<std::string> pop();
    bool insert(std::size_t index, const std::string& value);
    std::optional<std::string> remove(std::size_t index);
    std::optional<std::size_t> count();
    std::optional<std::string> get(std::size_t index);
    bool set(std::size_t index, const std::string& value);
    bool swap(std::size_t firstIndex, std::size_t secondIndex);
    bool clear();
    bool isConnected() const;

private:
    RemoteListStub m_stub;

    bool sendStatusCommand(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments = {});
    std::optional<std::string> sendValueCommand(RequestOpcode opcode,
                                                const std::vector<std::uint8_t>& arguments = {});
};


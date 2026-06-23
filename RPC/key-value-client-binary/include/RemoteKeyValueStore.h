#pragma once
#include "RemoteListStub.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Abstracts a list of strings, which secretly communicates with a list-server-binary instance
// where the real list is stored.
class RemoteKeyValueStore {
public:
    explicit RemoteKeyValueStore(const std::string& host = "127.0.0.1", int port = 9090);
    ~RemoteKeyValueStore() = default;

    RemoteKeyValueStore(const RemoteKeyValueStore&) = delete;
    RemoteKeyValueStore& operator=(const RemoteKeyValueStore&) = delete;
    RemoteKeyValueStore(RemoteKeyValueStore&&) = default;
    RemoteKeyValueStore& operator=(RemoteKeyValueStore&&) = default;

    bool put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool exists(const std::string& key);
    std::optional<std::vector<std::string>> keys();

    bool isConnected() const;

private:
    RemoteListStub m_stub;

    bool sendStatusCommand(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments = {});
    std::optional<std::string> sendValueCommand(RequestOpcode opcode,
                                                const std::vector<std::uint8_t>& arguments = {});
};


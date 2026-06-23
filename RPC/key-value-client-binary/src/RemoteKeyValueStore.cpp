#include "RemoteKeyValueStore.h"
#include "types.h"

#include <limits>

RemoteKeyValueStore::RemoteKeyValueStore(const std::string& host, int port)
    : m_stub{host, port} {
}

bool RemoteKeyValueStore::sendStatusCommand(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments) {
    auto response = m_stub.sendRequest(opcode, arguments);
    if (!response.has_value()) {
        return false;
    }

    return parseStatusResponse(response.value());
}

std::optional<std::string> RemoteKeyValueStore::sendValueCommand(RequestOpcode opcode,
                                                        const std::vector<std::uint8_t>& arguments) {
    auto response = m_stub.sendRequest(opcode, arguments);
    if (!response.has_value()) {
        return std::nullopt;
    }

    return parseValueResponse(response.value());
}

// START HERE: to push a string into the list, we send a message with a Push opcode
// and the string argument. Since PUSH responds with OK if the push succeeds,
// we parse that response as a bool value and return it.

//PUT
bool RemoteKeyValueStore::put(const std::string& key, const std::string& value) {
    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, key)) {
        return false;
    }

    if (!appendString(arguments, value)) {
        return false;
    }

    return sendStatusCommand(RequestOpcode::Put, arguments);
}

//GET
std::optional<std::string> RemoteKeyValueStore::get(const std::string& key) {
    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, key)) {
        return std::nullopt;
    }
    return sendValueCommand(RequestOpcode::Get, arguments);
}

//EXISTS
bool RemoteKeyValueStore::exists(const std::string& key) {
    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, key)) {
        return false;
    }
    return sendStatusCommand(RequestOpcode::Exists, arguments);
}

//KEYS
std::optional<std::vector<std::string>> RemoteKeyValueStore::keys() {
    auto response = m_stub.sendRequest(RequestOpcode::Keys, {});
    if (!response.has_value()) {
        return std::nullopt;
    }
    return parseKeysResponse(response.value());
}


bool RemoteKeyValueStore::isConnected() const {
    return m_stub.isConnected();
}

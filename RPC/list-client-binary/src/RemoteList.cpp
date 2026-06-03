#include "RemoteList.h"
#include "types.h"

#include <limits>

RemoteList::RemoteList(const std::string& host, int port)
    : m_stub{host, port} {
}

bool RemoteList::sendStatusCommand(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments) {
    auto response = m_stub.sendRequest(opcode, arguments);
    if (!response.has_value()) {
        return false;
    }

    return parseStatusResponse(response.value());
}

std::optional<std::string> RemoteList::sendValueCommand(RequestOpcode opcode,
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
bool RemoteList::push(const std::string& value) {
    // Encode the bytes of the string argument, including its length.
    std::vector<std::uint8_t> arguments{};
    if (!appendString(arguments, value)) {
        return false;
    }

    // Send and parse a PUSH command, expecting to receive back an OK ("Status") response.
    return sendStatusCommand(RequestOpcode::Push, arguments);
}

std::optional<std::string> RemoteList::pop() {
    // Send and parse a POP command, expecting to receive back a string VALUE response.
    return sendValueCommand(RequestOpcode::Pop);
}

bool RemoteList::insert(std::size_t index, const std::string& value) {
    if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    std::vector<std::uint8_t> arguments{};
    appendInt32(arguments, static_cast<std::int32_t>(index));
    if (!appendString(arguments, value)) {
        return false;
    }

    return sendStatusCommand(RequestOpcode::Insert, arguments);
}

std::optional<std::string> RemoteList::remove(std::size_t index) {
    if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> arguments{};
    appendInt32(arguments, static_cast<std::int32_t>(index));
    return sendValueCommand(RequestOpcode::Remove, arguments);
}

std::optional<std::size_t> RemoteList::count() {
    auto response = m_stub.sendRequest(RequestOpcode::Count, {});
    if (!response.has_value()) {
        return std::nullopt;
    }

    return parseCountResponse(response.value());
}

std::optional<std::string> RemoteList::get(std::size_t index) {
    if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> arguments{};
    appendInt32(arguments, static_cast<std::int32_t>(index));
    return sendValueCommand(RequestOpcode::Get, arguments);
}

bool RemoteList::set(std::size_t index, const std::string& value) {
    if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    std::vector<std::uint8_t> arguments{};
    appendInt32(arguments, static_cast<std::int32_t>(index));
    if (!appendString(arguments, value)) {
        return false;
    }

    return sendStatusCommand(RequestOpcode::Set, arguments);
}

bool RemoteList::swap(std::size_t firstIndex, std::size_t secondIndex) {
    if (firstIndex > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
        || secondIndex > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    std::vector<std::uint8_t> arguments{};
    appendInt32(arguments, static_cast<std::int32_t>(firstIndex));
    appendInt32(arguments, static_cast<std::int32_t>(secondIndex));
    return sendStatusCommand(RequestOpcode::Swap, arguments);
}

bool RemoteList::clear() {
    return sendStatusCommand(RequestOpcode::Clear);
}

bool RemoteList::isConnected() const {
    return m_stub.isConnected();
}

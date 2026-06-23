#include "MessageReader.h"

#include <arpa/inet.h>
#include <cstring>
#include <limits>

std::optional<std::uint8_t> MessageReader::readByte() {
    if (offset + sizeof(std::uint8_t) > buffer.size()) {
        return std::nullopt;
    }

    std::uint8_t networkValue{};
    std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
    offset += sizeof(networkValue);
    return networkValue;
}

std::optional<std::int32_t> MessageReader::readInt32() {
    if (offset + sizeof(std::uint32_t) > buffer.size()) {
        return std::nullopt;
    }

    std::uint32_t networkValue{};
    std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
    offset += sizeof(networkValue);
    return static_cast<std::int32_t>(ntohl(networkValue));
}

std::optional<std::string> MessageReader::readString() {
    std::optional<std::int32_t> length{readInt32()};
    if (!length.has_value() || length.value() < 0) {
        return std::nullopt;
    }

    const std::size_t stringLength{static_cast<std::size_t>(length.value())};
    if (offset + stringLength > buffer.size()) {
        return std::nullopt;
    }

    std::string value{
        reinterpret_cast<const char*>(buffer.data() + offset),
        stringLength
    };
    offset += stringLength;
    return value;
}

bool MessageReader::isAtEnd() const {
    return offset == buffer.size();
}
#pragma once

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

struct MessageReader {
    const std::vector<std::uint8_t>& buffer;
    std::size_t offset{0};

    std::optional<std::int32_t> readInt32() {
        if (offset + sizeof(std::uint32_t) > buffer.size()) {
            return std::nullopt;
        }

        std::uint32_t networkValue{};
        std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
        offset += sizeof(networkValue);
        return static_cast<std::int32_t>(ntohl(networkValue));
    }

    std::optional<std::uint8_t> readByte() {
        if (offset + sizeof(std::uint8_t) > buffer.size()) {
            return std::nullopt;
        }

        std::uint8_t networkValue{};
        std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
        offset += sizeof(networkValue);
        return networkValue;
    }

    std::optional<std::string> readString() {
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

    bool isAtEnd() const {
        return offset == buffer.size();
    }
};

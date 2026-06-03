#pragma once
#include <vector>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <arpa/inet.h>
#include <cstring>

// A MessageReader takes the binary contents of a message (excluding the message length),
// and decodes individual integers and strings from the message. It is used to read arguments
// when the opcode is already known.
struct MessageReader {
    // The buffer of bytes to decode.
    const std::vector<std::uint8_t>& buffer;
    // The position in the buffer to start the next read operation.
    std::size_t offset{0};

    // Read a 32-bit signed integer.
    std::optional<std::int32_t> readInt32() {
        // Make sure there are enough bytes to read an integer.
        if (offset + sizeof(std::uint32_t) > buffer.size()) {
            return std::nullopt;
        }

        // Copy 4 bytes from the buffer into a uint32_t.
        std::uint32_t networkValue{};
        std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
        // Keep track that we just read 4 more bytes.
        offset += sizeof(networkValue);

        // Convert the 4-byte value from network order to host order.
        return static_cast<std::int32_t>(ntohl(networkValue));
    }

    // Read an 8-bit value.
    std::optional<std::uint8_t> readByte() {
        if (offset + sizeof(std::uint8_t) > buffer.size()) {
            return std::nullopt;
        }

        std::uint8_t networkValue{};
        std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
        // Keep track that we just read 4 more bytes.
        offset += sizeof(networkValue);

        // Convert the 4-byte value from network order to host order.
        return networkValue;
    }

    // Read a string.
    std::optional<std::string> readString() {
        // First read the string length.
        std::optional<std::int32_t> length{readInt32()};
        if (!length.has_value() || length.value() < 0) {
            return std::nullopt;
        }

        const std::size_t stringLength{static_cast<std::size_t>(length.value())};
        if (offset + stringLength > buffer.size()) {
            return std::nullopt;
        }

        // Construct an std::string using the appropriate bytes from the buffer.
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

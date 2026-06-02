#ifndef REQUEST_HANDLERS_H
#define REQUEST_HANDLERS_H

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct SharedStore {
    std::vector<std::string> values{};
    std::mutex mutex{};
};

enum class RequestOpcode : std::uint8_t {
    Push = 1,
    Pop = 2,
    Insert = 3,
    Remove = 4,
    Count = 5,
    Get = 6,
    Set = 7,
    Swap = 8,
    Clear = 9,
    Quit = 10
};

enum class ResponseOpcode : std::uint8_t {
    Ok = 64,
    Value = 65,
    Count = 66,
    Bye = 67,
    Error = 127
};

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

std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode);
std::vector<std::uint8_t> buildErrorResponse(const std::string& error);
std::vector<std::uint8_t> buildValueResponse(const std::string& value);
std::vector<std::uint8_t> buildCountResponse(std::size_t count);

std::vector<std::uint8_t> handlePushRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handlePopRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleInsertRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleRemoveRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleCountRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleGetRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleSetRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleSwapRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleClearRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore& store);

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        const std::vector<std::uint8_t>& arguments,
                                        SharedStore& store);

#endif // REQUEST_HANDLERS_H

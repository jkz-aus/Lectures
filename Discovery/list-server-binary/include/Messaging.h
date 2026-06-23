#pragma once
#include <vector>
#include <cinttypes>
#include <optional>
#include <string>
template <typename TOpcode>
struct BinaryMessage {
    TOpcode opcode;
    std::vector<std::uint8_t> payload;
};

template<typename TOpcode>
std::optional<BinaryMessage<TOpcode>> parseMessage(const std::vector<std::uint8_t>& message) {
    if (message.empty()) {
        return std::nullopt;
    }

    return BinaryMessage<TOpcode>{
        static_cast<TOpcode>(message.front()),
        std::vector<std::uint8_t>(message.begin() + 1, message.end())
    };
}

std::vector<std::uint8_t> frameSuppressibleMessage(std::int32_t clientId, std::int32_t messageId,
    const std::vector<uint8_t>& requestPayload);


bool appendInt32(std::vector<std::uint8_t>& payload, std::int32_t value);
bool appendString(std::vector<std::uint8_t>& payload, const std::string& value);
std::vector<std::uint8_t> frameResponseMessage(const std::vector<std::uint8_t>& payload);
#include "ResponseHandlers.h"
#include <arpa/inet.h>

// Encode a 4-byte integer in network order and append it to a byte buffer.
bool appendInt32(std::vector<std::uint8_t>& payload, std::int32_t value) {
    const std::uint32_t networkValue{htonl(static_cast<std::uint32_t>(value))};
    // Take a pointer to the start of the integer.
    const auto* bytes{reinterpret_cast<const std::uint8_t*>(&networkValue)};
    // Copy 4 bytes from where the pointer begins into the payload vector.
    payload.insert(payload.end(), bytes, bytes + sizeof(networkValue));
    return true;
}

// Encode a length-prefixed string and append it to a byte buffer.
bool appendString(std::vector<std::uint8_t>& payload, const std::string& value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    // Append the string length.
    appendInt32(payload, static_cast<std::int32_t>(value.size()));
    // Copy the bytes of the string to the buffer.
    payload.insert(payload.end(), value.begin(), value.end());
    return true;
}

// Builds a payload consisting solely of an opcode.
std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode) {
    return {static_cast<std::uint8_t>(opcode)};
}

std::vector<std::uint8_t> buildClientIdResponse(std::int32_t clientId) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::ClientId)};
    appendInt32(payload, clientId);
    return payload;
}

// Builds a payload for an ERROR response.
std::vector<std::uint8_t> buildErrorResponse(const std::string& error) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Error)};
    appendString(payload, error);
    return payload;
}

// Builds a payload for a VALUE response.
std::vector<std::uint8_t> buildValueResponse(const std::string& value) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Value)};
    appendString(payload, value);
    return payload;
}

// Builds a payload for a COUNT response.
std::vector<std::uint8_t> buildCountResponse(std::size_t count) {
    if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return buildErrorResponse("count overflow");
    }

    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Count)};
    appendInt32(payload, static_cast<std::int32_t>(count));
    return payload;
}

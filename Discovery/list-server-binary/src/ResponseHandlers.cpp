#include "ResponseHandlers.h"
#include <arpa/inet.h>

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

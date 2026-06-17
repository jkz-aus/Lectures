#include "Messaging.h"

#include <limits>
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


// Frame a payload of bytes as a network message, prefixed with the payload's length
std::vector<std::uint8_t> frameResponseMessage(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> framed{};
    // Reserve enough space in the new buffer.
    framed.reserve(sizeof(std::uint32_t) + payload.size());

    // Encode the length of the payload.
    const std::uint32_t networkLength{htonl(static_cast<std::uint32_t>(payload.size()))};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};
    // Copy the length to the buffer.
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
    // Copy the payload to the buffer.
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

// Frame a payload into a suppressible message, with a given clientId and messageId that the
// receiving server might suppress due to duplication of the message ID.
std::vector<std::uint8_t> frameSuppressibleMessage(std::int32_t clientId, std::int32_t messageId,
    const std::vector<uint8_t>& requestPayload) {
    std::vector<std::uint8_t> message{};
    appendInt32(message, clientId);
    appendInt32(message, messageId);
    message.insert(message.end(), requestPayload.begin(), requestPayload.end());
    return frameResponseMessage(message);
}
#include "types.h"
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

bool appendInt32(std::vector<std::uint8_t>& payload, std::int32_t value) {
    const std::uint32_t networkValue{htonl(static_cast<std::uint32_t>(value))};
    const auto* bytes{reinterpret_cast<const std::uint8_t*>(&networkValue)};
    payload.insert(payload.end(), bytes, bytes + sizeof(networkValue));
    return true;
}

bool appendString(std::vector<std::uint8_t>& payload, const std::string& value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    appendInt32(payload, static_cast<std::int32_t>(value.size()));
    payload.insert(payload.end(), value.begin(), value.end());
    return true;
}

// Builds a message buffer according to the application protocol: first, an integer length
// of the payload; then the bytes of the payload, which is itself a 1-byte opcode and variable
// number of encoded arguments.
std::vector<std::uint8_t> frameRequest(std::int32_t clientId,
                                       std::int32_t messageId,
                                       RequestOpcode opcode,
                                       const std::vector<std::uint8_t>& arguments) {
    std::vector<std::uint8_t> framed{};
    const std::uint32_t payloadSize{
        static_cast<std::uint32_t>((2 * sizeof(std::int32_t)) + sizeof(std::uint8_t) + arguments.size())
    };
    const std::uint32_t networkLength{htonl(payloadSize)};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};

    framed.reserve(sizeof(networkLength) + payloadSize);
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
    appendInt32(framed, clientId);
    appendInt32(framed, messageId);
    framed.push_back(static_cast<std::uint8_t>(opcode));
    framed.insert(framed.end(), arguments.begin(), arguments.end());
    return framed;
}

std::optional<BinaryResponse> parseResponseMessage(const std::vector<std::uint8_t>& message) {
    if (message.empty()) {
        return std::nullopt;
    }

    BinaryResponse response{
        static_cast<ResponseOpcode>(message.front()),
        std::vector<std::uint8_t>(message.begin() + 1, message.end())
    };
    return response;
}

std::optional<std::int32_t> parseClientIdResponse(const BinaryResponse& response) {
    if (response.opcode != ResponseOpcode::ClientId) {
        return std::nullopt;
    }

    MessageReader reader{response.payload};
    std::optional<std::int32_t> clientId{reader.readInt32()};
    if (!clientId.has_value() || !reader.isAtEnd()) {
        return std::nullopt;
    }

    return clientId;
}

bool parseStatusResponse(const BinaryResponse& response) {
    return response.opcode == ResponseOpcode::Ok;
}

std::optional<std::string> parseValueResponse(const BinaryResponse& response) {
    if (response.opcode != ResponseOpcode::Value) {
        return std::nullopt;
    }

    MessageReader reader{response.payload};
    std::optional<std::string> value{reader.readString()};
    if (!value.has_value() || !reader.isAtEnd()) {
        return std::nullopt;
    }

    return value;
}

std::optional<std::size_t> parseCountResponse(const BinaryResponse& response) {
    if (response.opcode != ResponseOpcode::Count) {
        return std::nullopt;
    }

    MessageReader reader{response.payload};
    std::optional<std::int32_t> count{reader.readInt32()};
    if (!count.has_value() || count.value() < 0 || !reader.isAtEnd()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(count.value());
}

bool isErrorResponse(const BinaryResponse& response) {
    return response.opcode == ResponseOpcode::Error;
}

std::string getErrorMessage(const BinaryResponse& response) {
    if (!isErrorResponse(response)) {
        return {};
    }

    MessageReader reader{response.payload};
    std::optional<std::string> message{reader.readString()};
    if (!message.has_value() || !reader.isAtEnd()) {
        return "malformed error response";
    }

    return message.value();
}

#include "types.h"

#include <arpa/inet.h>
#include <cstring>
#include <limits>

std::optional<std::int32_t> BinaryReader::readInt32() {
    if (offset + sizeof(std::uint32_t) > buffer.size()) {
        return std::nullopt;
    }

    std::uint32_t networkValue{};
    std::memcpy(&networkValue, buffer.data() + offset, sizeof(networkValue));
    offset += sizeof(networkValue);
    return static_cast<std::int32_t>(ntohl(networkValue));
}

std::optional<std::string> BinaryReader::readString() {
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

bool BinaryReader::isAtEnd() const {
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

std::vector<std::uint8_t> frameRequest(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments) {
    std::vector<std::uint8_t> framed{};
    const std::uint32_t payloadSize{static_cast<std::uint32_t>(1 + arguments.size())};
    const std::uint32_t networkLength{htonl(payloadSize)};
    const auto* lengthBytes{reinterpret_cast<const std::uint8_t*>(&networkLength)};

    framed.reserve(sizeof(networkLength) + payloadSize);
    framed.insert(framed.end(), lengthBytes, lengthBytes + sizeof(networkLength));
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

bool parseStatusResponse(const BinaryResponse& response) {
    return response.opcode == ResponseOpcode::Ok;
}

std::optional<std::string> parseValueResponse(const BinaryResponse& response) {
    if (response.opcode != ResponseOpcode::Value) {
        return std::nullopt;
    }

    BinaryReader reader{response.payload};
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

    BinaryReader reader{response.payload};
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

    BinaryReader reader{response.payload};
    std::optional<std::string> message{reader.readString()};
    if (!message.has_value() || !reader.isAtEnd()) {
        return "malformed error response";
    }

    return message.value();
}

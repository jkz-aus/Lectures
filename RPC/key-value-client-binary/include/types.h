#ifndef TYPES_H
#define TYPES_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class RequestOpcode : std::uint8_t {
    Put = 0x01,
    Delete = 0x03,
    Count = 0x05,
    Get = 0x06,
    Exists = 0x07,
    Keys = 0x08,
    Quit = 0x0A
};

enum class ResponseOpcode : std::uint8_t {
    Ok = 0x40,
    Value = 0x41,
    Count = 0x42,
    Keys = 0x43,
    Bye = 0x45,
    Error = 0x7F
};

struct BinaryResponse {
    ResponseOpcode opcode;
    std::vector<std::uint8_t> payload;
};


bool appendInt32(std::vector<std::uint8_t>& payload, std::int32_t value);
bool appendString(std::vector<std::uint8_t>& payload, const std::string& value);
std::vector<std::uint8_t> frameRequest(RequestOpcode opcode, const std::vector<std::uint8_t>& arguments);
std::optional<BinaryResponse> parseResponseMessage(const std::vector<std::uint8_t>& message);

bool parseStatusResponse(const BinaryResponse& response);
std::optional<std::string> parseValueResponse(const BinaryResponse& response);
std::optional<std::size_t> parseCountResponse(const BinaryResponse& response);
bool isErrorResponse(const BinaryResponse& response);
std::string getErrorMessage(const BinaryResponse& response);

std::optional<std::vector<std::string>> parseKeysResponse (const BinaryResponse& response);
#endif // TYPES_H

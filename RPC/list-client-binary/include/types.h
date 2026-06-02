#ifndef TYPES_H
#define TYPES_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

struct BinaryResponse {
    ResponseOpcode opcode;
    std::vector<std::uint8_t> payload;
};

struct BinaryReader {
    const std::vector<std::uint8_t>& buffer;
    std::size_t offset{0};

    std::optional<std::int32_t> readInt32();
    std::optional<std::string> readString();
    bool isAtEnd() const;
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

#endif // TYPES_H

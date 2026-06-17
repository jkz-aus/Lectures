#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include "Messaging.h"

enum class ResponseOpcode : std::uint8_t {
    Ok = 0x40,
    Value = 0x41,
    Count = 0x42,
    Bye = 0x43,
    ClientId = 0x44,
    Error = 0x7F
};

std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode);
std::vector<std::uint8_t> buildClientIdResponse(std::int32_t clientId);
std::vector<std::uint8_t> buildErrorResponse(const std::string& error);
std::vector<std::uint8_t> buildValueResponse(const std::string& value);
std::vector<std::uint8_t> buildCountResponse(std::size_t count);


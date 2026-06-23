#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "SharedRegistry.h"

enum class ResponseOpcode : std::uint8_t {
    Ok = 0x40,
    Provider = 0x41,
    Bye = 0x43,
    ClientId = 0x44,
    Error = 0x7F
};

std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode);
std::vector<std::uint8_t> buildClientIdResponse(std::int32_t clientId);
std::vector<std::uint8_t> buildErrorResponse(const std::string& error);
std::vector<std::uint8_t> buildProviderResponse(const ServiceProvider& provider);

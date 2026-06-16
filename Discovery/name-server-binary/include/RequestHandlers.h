#pragma once

#include <cstdint>
#include <vector>

#include "MessageReader.h"
#include "SharedRegistry.h"

enum class RequestOpcode : std::uint8_t {
    Register = 0x01,
    Unregister = 0x02,
    Heartbeat = 0x03,
    Resolve = 0x04,
    Quit = 0x0A
};

std::vector<std::uint8_t> handleRegisterRequest(MessageReader& reader, SharedRegistry& registry);
std::vector<std::uint8_t> handleUnregisterRequest(MessageReader& reader, SharedRegistry& registry);
std::vector<std::uint8_t> handleHeartbeatRequest(MessageReader& reader, SharedRegistry& registry);
std::vector<std::uint8_t> handleResolveRequest(MessageReader& reader, SharedRegistry& registry);
std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedRegistry& registry);

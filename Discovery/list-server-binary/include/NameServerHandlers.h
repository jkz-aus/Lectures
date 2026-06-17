#pragma once
#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include "MessageReader.h"
#include "Messaging.h"

enum class NameServerRequestOpcode : std::uint8_t {
    Register = 0x01,
    Unregister = 0x02,
    Heartbeat = 0x03,
    Resolve = 0x04,
    Quit = 0x0A
};

enum class NameServerResponseOpcode : std::uint8_t {
    Ok = 0x40,
    ClientId = 0x44,
    Error = 0x7F
};

std::vector<std::uint8_t> buildRegisterRequest(const std::string& serviceName,
    const std::string& providerId, const std::string& address, std::int32_t port);

// TODO: you must implement this function to build a payload for a Heartbeat request.
// The Heartbeat opcode expects two string arguments: the service name, and the provider identifier.
// See the implementation of buildRegisterRequest for an example.
std::vector<std::uint8_t> buildHeartbeatRequest(const std::string& serviceName,
    const std::string& providerId);

#include "NameServerHandlers.h"
#include <optional>
std::vector<std::uint8_t> buildRegisterRequest(const std::string& serviceName,
    const std::string& providerId, const std::string& address, std::int32_t port) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(NameServerRequestOpcode::Register)};
    appendString(payload, serviceName);
    appendString(payload, providerId);
    appendString(payload, address);
    appendInt32(payload, port);
    return payload;
}

#include "ResponseHandlers.h"

#include <arpa/inet.h>
#include <limits>

namespace {

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

}

std::vector<std::uint8_t> buildStatusResponse(ResponseOpcode opcode) {
    return {static_cast<std::uint8_t>(opcode)};
}

std::vector<std::uint8_t> buildClientIdResponse(std::int32_t clientId) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::ClientId)};
    appendInt32(payload, clientId);
    return payload;
}

std::vector<std::uint8_t> buildErrorResponse(const std::string& error) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Error)};
    appendString(payload, error);
    return payload;
}

std::vector<std::uint8_t> buildProviderResponse(const ServiceProvider& provider) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(ResponseOpcode::Provider)};
    appendString(payload, provider.identifier);
    appendString(payload, provider.host);
    appendInt32(payload, provider.port);
    return payload;
}

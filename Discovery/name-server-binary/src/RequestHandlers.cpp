#include "RequestHandlers.h"

#include <algorithm>
#include <mutex>

#include "ResponseHandlers.h"

namespace {

bool isValidPort(std::int32_t port) {
    return port > 0 && port <= 65535;
}

std::vector<ServiceProvider>::iterator findProvider(std::vector<ServiceProvider>& providers,
                                                    const std::string& identifier) {
    return std::find_if(providers.begin(), providers.end(),
                        [&](const ServiceProvider& provider) {
                            return provider.identifier == identifier;
                        });
}

}

std::vector<std::uint8_t> handleRegisterRequest(MessageReader& reader, SharedRegistry& registry) {
    std::optional<std::string> serviceName{reader.readString()};
    std::optional<std::string> providerId{reader.readString()};
    std::optional<std::string> host{reader.readString()};
    std::optional<std::int32_t> port{reader.readInt32()};

    if (!serviceName.has_value() || serviceName->empty()
        || !providerId.has_value() || providerId->empty()
        || !host.has_value() || host->empty()
        || !port.has_value() || !isValidPort(port.value())
        || !reader.isAtEnd()) {
        return buildErrorResponse("REGISTER requires service name, provider identifier, host, and port");
    }

    {
        std::lock_guard<std::mutex> lock{registry.mutex};
        std::vector<ServiceProvider>& providers{registry.services[serviceName.value()]};
        auto existing{findProvider(providers, providerId.value())};
        if (existing == providers.end()) {
            providers.push_back(ServiceProvider{providerId.value(), host.value(), port.value()});
        } else {
            existing->host = host.value();
            existing->port = port.value();
        }
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleUnregisterRequest(MessageReader& reader, SharedRegistry& registry) {
    std::optional<std::string> serviceName{reader.readString()};
    std::optional<std::string> providerId{reader.readString()};

    if (!serviceName.has_value() || serviceName->empty()
        || !providerId.has_value() || providerId->empty()
        || !reader.isAtEnd()) {
        return buildErrorResponse("UNREGISTER requires service name and provider identifier");
    }

    {
        std::lock_guard<std::mutex> lock{registry.mutex};
        auto servicesIt{registry.services.find(serviceName.value())};
        if (servicesIt == registry.services.end()) {
            return buildErrorResponse("service not found");
        }

        auto& providers{servicesIt->second};
        auto providerIt{findProvider(providers, providerId.value())};
        if (providerIt == providers.end()) {
            return buildErrorResponse("provider not found");
        }

        providers.erase(providerIt);
        if (providers.empty()) {
            registry.services.erase(servicesIt);
        }
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleHeartbeatRequest(MessageReader& reader, SharedRegistry& registry) {
    std::optional<std::string> serviceName{reader.readString()};
    std::optional<std::string> providerId{reader.readString()};

    if (!serviceName.has_value() || serviceName->empty()
        || !providerId.has_value() || providerId->empty()
        || !reader.isAtEnd()) {
        return buildErrorResponse("HEARTBEAT requires service name and provider identifier");
    }

    {
        std::lock_guard<std::mutex> lock{registry.mutex};
        auto servicesIt{registry.services.find(serviceName.value())};
        if (servicesIt == registry.services.end()) {
            return buildErrorResponse("service not found");
        }

        auto providerIt{findProvider(servicesIt->second, providerId.value())};
        if (providerIt == servicesIt->second.end()) {
            return buildErrorResponse("provider not found");
        }
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleResolveRequest(MessageReader& reader, SharedRegistry& registry) {
    std::optional<std::string> serviceName{reader.readString()};
    if (!serviceName.has_value() || serviceName->empty() || !reader.isAtEnd()) {
        return buildErrorResponse("RESOLVE requires service name");
    }

    ServiceProvider provider{};
    {
        std::lock_guard<std::mutex> lock{registry.mutex};
        auto servicesIt{registry.services.find(serviceName.value())};
        if (servicesIt == registry.services.end() || servicesIt->second.empty()) {
            return buildErrorResponse("service not found");
        }

        provider = servicesIt->second.front();
    }

    return buildProviderResponse(provider);
}

std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedRegistry&) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("QUIT takes no arguments");
    }

    return buildStatusResponse(ResponseOpcode::Bye);
}

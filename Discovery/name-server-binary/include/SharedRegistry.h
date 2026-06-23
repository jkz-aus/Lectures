#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ServiceProvider {
    std::string identifier{};
    std::string host{};
    std::int32_t port{};
};

struct SharedRegistry {
    std::unordered_map<std::string, std::vector<ServiceProvider>> services{};
    std::mutex mutex{};
};

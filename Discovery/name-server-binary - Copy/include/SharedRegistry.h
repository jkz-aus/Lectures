#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

struct ServiceProvider {
    std::string identifier{};
    std::string host{};
    std::int32_t port{};
    std::chrono::system_clock::time_point lastHeartbeat{std::chrono::system_clock::now()};
};

struct SharedRegistry {
    std::unordered_map<std::string, std::vector<ServiceProvider>> services{};
    std::mutex mutex{};
};

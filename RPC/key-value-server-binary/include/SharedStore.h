#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

struct SharedStore {
    std::unordered_map<std::string, std::string> values{};
    std::mutex mutex{};
};
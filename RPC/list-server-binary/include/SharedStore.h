#pragma once

struct SharedStore {
    std::vector<std::string> values{};
    std::mutex mutex{};
};
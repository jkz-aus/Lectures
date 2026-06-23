#pragma once

struct SharedStore {
    std::vector<std::string> sharedList{};
    std::mutex mutex{};
};
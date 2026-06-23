#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
struct MessageReader {
    const std::vector<std::uint8_t>& buffer;
    std::size_t offset{0};

    std::optional<std::int32_t> readInt32();
    std::optional<std::string> readString();
    std::optional<std::uint8_t> readByte();
    bool isAtEnd() const;
};
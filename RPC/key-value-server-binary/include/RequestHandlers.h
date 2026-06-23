#pragma once
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "SharedStore.h"
#include "MessageReader.h"

enum class RequestOpcode : std::uint8_t {
    Put = 0x01,
    Delete = 0x03,
    Count = 0x05,
    Get = 0x06,
    Exists = 0x07,
    Keys = 0x08,
    Quit = 0x0A
};

std::vector<std::uint8_t> handlePutRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleGetRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleDeleteRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleCountRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleExistsRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleKeysRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore& store);
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
    Push = 0x01,
    Pop = 0x02,
    Insert = 0x03,
    Remove = 0x04,
    Count = 0x05,
    Get = 0x06,
    Set = 0x07,
    Swap = 0x08,
    Clear = 0x09,
    Quit = 0x0A
};
std::vector<std::uint8_t> handlePushRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handlePopRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleInsertRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleRemoveRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleCountRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleGetRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleSetRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleSwapRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleClearRequest(MessageReader& reader, SharedStore& store);
std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore& store);

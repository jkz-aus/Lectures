#include "request_handlers.h"

#include <utility>

std::vector<std::uint8_t> handlePushRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::string> value{reader.readString()};
    if (!value.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("PUSH requires value");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        store.values.push_back(value.value());
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handlePopRequest(MessageReader& reader, SharedStore& store) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("POP takes no arguments");
    }

    std::string value{};
    {
        std::lock_guard<std::mutex> lock{store.mutex};
        if (store.values.empty()) {
            return buildErrorResponse("list is empty");
        }

        value = store.values.back();
        store.values.pop_back();
    }

    return buildValueResponse(value);
}

std::vector<std::uint8_t> handleInsertRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::int32_t> index{reader.readInt32()};
    std::optional<std::string> value{reader.readString()};
    if (!index.has_value() || !value.has_value() || index.value() < 0 || !reader.isAtEnd()) {
        return buildErrorResponse("INSERT requires index and value");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        const std::size_t insertIndex{static_cast<std::size_t>(index.value())};
        if (insertIndex > store.values.size()) {
            return buildErrorResponse("invalid index");
        }

        store.values.insert(store.values.begin() + static_cast<std::ptrdiff_t>(insertIndex), value.value());
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleRemoveRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::int32_t> index{reader.readInt32()};
    if (!index.has_value() || index.value() < 0 || !reader.isAtEnd()) {
        return buildErrorResponse("REMOVE requires index");
    }

    std::string value{};
    {
        std::lock_guard<std::mutex> lock{store.mutex};
        const std::size_t removeIndex{static_cast<std::size_t>(index.value())};
        if (removeIndex >= store.values.size()) {
            return buildErrorResponse("invalid index");
        }

        value = store.values[removeIndex];
        store.values.erase(store.values.begin() + static_cast<std::ptrdiff_t>(removeIndex));
    }

    return buildValueResponse(value);
}

std::vector<std::uint8_t> handleCountRequest(MessageReader& reader, SharedStore& store) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("COUNT takes no arguments");
    }

    std::size_t count{};
    {
        std::lock_guard<std::mutex> lock{store.mutex};
        count = store.values.size();
    }

    return buildCountResponse(count);
}

std::vector<std::uint8_t> handleGetRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::int32_t> index{reader.readInt32()};
    if (!index.has_value() || index.value() < 0 || !reader.isAtEnd()) {
        return buildErrorResponse("GET requires index");
    }

    std::string value{};
    {
        std::lock_guard<std::mutex> lock{store.mutex};
        const std::size_t getIndex{static_cast<std::size_t>(index.value())};
        if (getIndex >= store.values.size()) {
            return buildErrorResponse("invalid index");
        }

        value = store.values[getIndex];
    }

    return buildValueResponse(value);
}

std::vector<std::uint8_t> handleSetRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::int32_t> index{reader.readInt32()};
    std::optional<std::string> value{reader.readString()};
    if (!index.has_value() || !value.has_value() || index.value() < 0 || !reader.isAtEnd()) {
        return buildErrorResponse("SET requires index and value");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        const std::size_t setIndex{static_cast<std::size_t>(index.value())};
        if (setIndex >= store.values.size()) {
            return buildErrorResponse("invalid index");
        }

        store.values[setIndex] = value.value();
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleSwapRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::int32_t> firstIndex{reader.readInt32()};
    std::optional<std::int32_t> secondIndex{reader.readInt32()};
    if (!firstIndex.has_value() || !secondIndex.has_value()
        || firstIndex.value() < 0 || secondIndex.value() < 0 || !reader.isAtEnd()) {
        return buildErrorResponse("SWAP requires two indices");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        const std::size_t left{static_cast<std::size_t>(firstIndex.value())};
        const std::size_t right{static_cast<std::size_t>(secondIndex.value())};
        if (left >= store.values.size() || right >= store.values.size()) {
            return buildErrorResponse("invalid index");
        }

        std::swap(store.values[left], store.values[right]);
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleClearRequest(MessageReader& reader, SharedStore& store) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("CLEAR takes no arguments");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        store.values.clear();
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore&) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("QUIT takes no arguments");
    }

    return buildStatusResponse(ResponseOpcode::Bye);
}

std::vector<std::uint8_t> handleRequest(RequestOpcode opcode,
                                        const std::vector<std::uint8_t>& arguments,
                                        SharedStore& store) {
    MessageReader reader{arguments};

    switch (opcode) {
        case RequestOpcode::Push:
            return handlePushRequest(reader, store);
        case RequestOpcode::Pop:
            return handlePopRequest(reader, store);
        case RequestOpcode::Insert:
            return handleInsertRequest(reader, store);
        case RequestOpcode::Remove:
            return handleRemoveRequest(reader, store);
        case RequestOpcode::Count:
            return handleCountRequest(reader, store);
        case RequestOpcode::Get:
            return handleGetRequest(reader, store);
        case RequestOpcode::Set:
            return handleSetRequest(reader, store);
        case RequestOpcode::Swap:
            return handleSwapRequest(reader, store);
        case RequestOpcode::Clear:
            return handleClearRequest(reader, store);
        case RequestOpcode::Quit:
            return handleQuitRequest(reader, store);
    }

    return buildErrorResponse("unknown opcode");
}

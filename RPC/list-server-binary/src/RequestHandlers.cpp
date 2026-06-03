#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include <utility>

std::vector<std::uint8_t> handlePushRequest(MessageReader& reader, SharedStore& store) {
    // Push: 1 string argument.
    std::optional<std::string> argument{reader.readString()};
    if (!argument.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("PUSH requires value");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        store.sharedList.push_back(argument.value());
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
        if (store.sharedList.empty()) {
            return buildErrorResponse("list is empty");
        }

        value = store.sharedList.back();
        store.sharedList.pop_back();
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
        if (insertIndex > store.sharedList.size()) {
            return buildErrorResponse("invalid index");
        }

        store.sharedList.insert(store.sharedList.begin() + static_cast<std::ptrdiff_t>(insertIndex), value.value());
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
        if (removeIndex >= store.sharedList.size()) {
            return buildErrorResponse("invalid index");
        }

        value = store.sharedList[removeIndex];
        store.sharedList.erase(store.sharedList.begin() + static_cast<std::ptrdiff_t>(removeIndex));
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
        count = store.sharedList.size();
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
        if (getIndex >= store.sharedList.size()) {
            return buildErrorResponse("invalid index");
        }

        value = store.sharedList[getIndex];
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
        if (setIndex >= store.sharedList.size()) {
            return buildErrorResponse("invalid index");
        }

        store.sharedList[setIndex] = value.value();
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
        if (left >= store.sharedList.size() || right >= store.sharedList.size()) {
            return buildErrorResponse("invalid index");
        }

        std::swap(store.sharedList[left], store.sharedList[right]);
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleClearRequest(MessageReader& reader, SharedStore& store) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("CLEAR takes no arguments");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        store.sharedList.clear();
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore&) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("QUIT takes no arguments");
    }

    return buildStatusResponse(ResponseOpcode::Bye);
}

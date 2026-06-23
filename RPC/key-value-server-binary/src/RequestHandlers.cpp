#include "RequestHandlers.h"
#include "ResponseHandlers.h"
#include <utility>
#include <algorithm>
//put
std::vector<std::uint8_t> handlePutRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::string> key{reader.readString()};
    std::optional<std::string> value{reader.readString()};

    if (!key.has_value() || !value.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("PUT requires key and value");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        store.values[key.value()] = value.value();
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

//get
std::vector<std::uint8_t> handleGetRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::string> key{reader.readString()};

    if (!key.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("GET requires key");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        auto it{store.values.find(key.value())};
        if (it == store.values.end()) {
            return buildStatusResponse(ResponseOpcode::NotFound);
        }
        return buildValueResponse(it->second);
    }
}

//delete
std::vector<std::uint8_t> handleDeleteRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::string> key{reader.readString()};
    if (!key.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("DELETE requires key");
    }

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        auto erased{store.values.erase(key.value())};
        if (erased == 0) {
            return buildStatusResponse(ResponseOpcode::NotFound);
        }
    }

    return buildStatusResponse(ResponseOpcode::Ok);
}

//count
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

//exists
std::vector<std::uint8_t> handleExistsRequest(MessageReader& reader, SharedStore& store) {
    std::optional<std::string> key{reader.readString()};

    if (!key.has_value() || !reader.isAtEnd()) {
        return buildErrorResponse("EXISTS requires key");
    }
    {
        std::lock_guard<std::mutex> lock{store.mutex};
        if (store.values.find(key.value()) == store.values.end()) {
            return buildStatusResponse(ResponseOpcode::NotFound);
        }
    }
    return buildStatusResponse(ResponseOpcode::Ok);
}

//keys
std::vector<std::uint8_t> handleKeysRequest(MessageReader& reader, SharedStore& store) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("KEYS requires no arguments");
    }
    std::vector<std::string> keys{};

    {
        std::lock_guard<std::mutex> lock{store.mutex};
        for (const auto& pair : store.values) {
            keys.push_back(pair.first);
        }
    }
    std::sort(keys.begin(), keys.end());
    
    return buildKeysResponse(keys);
}

//quit
std::vector<std::uint8_t> handleQuitRequest(MessageReader& reader, SharedStore&) {
    if (!reader.isAtEnd()) {
        return buildErrorResponse("QUIT takes no arguments");
    }

    return buildStatusResponse(ResponseOpcode::Bye);
}

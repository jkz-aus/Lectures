#include "remote_list.h"
#include "types.h"
#include <iostream>

RemoteList::RemoteList(const std::string& host, int port)
    : stub_(std::make_unique<RemoteListStub>(host, port)) {
}

bool RemoteList::sendStatusCommand(const std::string& command) {
    auto response = stub_->sendCommand(command);
    if (!response.has_value()) {
        return false;
    }
    return parseStatusResponse(response.value());
}

std::optional<std::string> RemoteList::sendValueCommand(const std::string& command) {
    auto response = stub_->sendCommand(command);
    if (!response.has_value()) {
        return std::nullopt;
    }
    return parseValueResponse(response.value());
}

bool RemoteList::push(const std::string& value) {
    return sendStatusCommand("PUSH " + value);
}

std::optional<std::string> RemoteList::pop() {
    return sendValueCommand("POP");
}

bool RemoteList::insert(std::size_t index, const std::string& value) {
    return sendStatusCommand("INSERT " + std::to_string(index) + " " + value);
}

std::optional<std::string> RemoteList::remove(std::size_t index) {
    return sendValueCommand("REMOVE " + std::to_string(index));
}

std::optional<std::size_t> RemoteList::count() {
    auto response = sendValueCommand("COUNT");
    if (!response.has_value()) {
        return std::nullopt;
    }
    try {
        return std::stoull(response.value());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> RemoteList::get(std::size_t index) {
    return sendValueCommand("GET " + std::to_string(index));
}

bool RemoteList::set(std::size_t index, const std::string& value) {
    return sendStatusCommand("SET " + std::to_string(index) + " " + value);
}

bool RemoteList::swap(std::size_t firstIndex, std::size_t secondIndex) {
    return sendStatusCommand("SWAP " + std::to_string(firstIndex) + " " + std::to_string(secondIndex));
}

bool RemoteList::clear() {
    return sendStatusCommand("CLEAR");
}

bool RemoteList::isConnected() const {
    return stub_ && stub_->isConnected();
}

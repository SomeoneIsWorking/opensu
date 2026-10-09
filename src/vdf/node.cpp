#include "node.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <ranges>
#include <sstream>
#include <utility>

namespace opensu::vdf {

Node Node::from(std::vector<std::pair<std::string, Value>> entries) {
    Node node;
    node.entries_ = std::move(entries);
    return node;
}

const Node::Value* Node::findValue(std::string_view key) const {
    for (const auto& [name, value] : entries_) {
        // Steam varies key casing between files, so lookups ignore case.
        const bool same = std::ranges::equal(name, key, [](char x, char y) {
            return std::tolower(static_cast<unsigned char>(x)) ==
                   std::tolower(static_cast<unsigned char>(y));
        });
        if (same) {
            return &value;
        }
    }
    return nullptr;
}

std::optional<Node::Value> Node::find(std::string_view key) const {
    if (const Value* value = findValue(key); value != nullptr) {
        return *value;
    }
    return std::nullopt;
}

bool Node::has(std::initializer_list<std::string_view> path) const {
    if (path.size() == 0) {
        return false;
    }
    const Node* current = this;
    auto key = path.begin();
    for (; key + 1 != path.end(); ++key) {
        const Value* value = current->findValue(*key);
        const Node* next = value != nullptr ? std::get_if<Node>(value) : nullptr;
        if (next == nullptr) {
            return false;
        }
        current = next;
    }
    return current->findValue(*key) != nullptr;
}

std::optional<Node> Node::block(std::initializer_list<std::string_view> path) const {
    const Node* current = this;
    for (const std::string_view key : path) {
        const Value* value = current->findValue(key);
        const Node* next = value != nullptr ? std::get_if<Node>(value) : nullptr;
        if (next == nullptr) {
            return std::nullopt;
        }
        current = next;
    }
    return *current;
}

std::optional<std::string> Node::str(std::initializer_list<std::string_view> path) const {
    const auto last = path.end();
    if (path.begin() == last) {
        return std::nullopt;
    }
    // Every key but the last must name a block; the last names the value.
    const Node* current = this;
    auto key = path.begin();
    for (; key + 1 != last; ++key) {
        const Value* value = current->findValue(*key);
        const Node* next = value != nullptr ? std::get_if<Node>(value) : nullptr;
        if (next == nullptr) {
            return std::nullopt;
        }
        current = next;
    }
    const Value* value = current->findValue(*key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::string* text = std::get_if<std::string>(value); text != nullptr) {
        return *text;
    }
    return std::nullopt;
}

std::optional<long long> Node::integer(std::initializer_list<std::string_view> path) const {
    const std::optional<std::string> text = str(path);
    if (!text) {
        return std::nullopt;
    }
    long long value = 0;
    const char* begin = text->data();
    const char* end = begin + text->size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string> Node::keys() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& [name, value] : entries_) {
        out.push_back(name);
    }
    std::ranges::sort(out);
    return out;
}

} // namespace opensu::vdf
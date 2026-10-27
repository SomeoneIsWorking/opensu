#include "steam_appinfo.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "lucent/log.h"

namespace opensu::library::steam {
namespace {

static_assert(std::endian::native == std::endian::little, "appinfo.vdf is little-endian");

/// Format 28 spells every key; 29 keeps keys in a string table at the end of the file.
constexpr std::uint32_t inlineKeys = 0x07564428;
constexpr std::uint32_t tableKeys = 0x07564429;
/// After an app's id and size: info state, last change, PICS token, SHA-1, change number, the
/// binary SHA-1.
constexpr std::size_t appPrefix = 4 + 4 + 8 + 20 + 4 + 20;
/// Deeper than any app Steam writes; stops a corrupt file recursing without end.
constexpr int maxDepth = 64;

enum class Type : std::uint8_t {
    Block = 0,
    String = 1,
    Int32 = 2,
    Float = 3,
    Pointer = 4,
    Color = 6,
    UInt64 = 7,
    End = 8,
    Int64 = 10,
    EndAlternate = 11,
};

struct Malformed : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Cursor {
  public:
    Cursor(std::string_view data, std::size_t at) : data_{data}, at_{at} {
    }

    template <typename T> [[nodiscard]] T fixed() {
        if (at_ > data_.size() || data_.size() - at_ < sizeof(T)) {
            throw Malformed{"ends early"};
        }
        T value{};
        std::memcpy(&value, data_.data() + at_, sizeof(T));
        at_ += sizeof(T);
        return value;
    }

    [[nodiscard]] std::string text() {
        const std::size_t end = data_.find('\0', at_);
        if (end == std::string_view::npos) {
            throw Malformed{"a string is not terminated"};
        }
        std::string value{data_.substr(at_, end - at_)};
        at_ = end + 1;
        return value;
    }

  private:
    std::string_view data_;
    std::size_t at_;
};

class Decoder {
  public:
    explicit Decoder(const std::vector<std::string>* keys) : keys_{keys} {
    }

    [[nodiscard]] vdf::Node block(Cursor& in, int depth) const {
        if (depth > maxDepth) {
            throw Malformed{"nested too deep"};
        }
        std::vector<std::pair<std::string, vdf::Node::Value>> entries;
        for (;;) {
            const auto type = static_cast<Type>(in.fixed<std::uint8_t>());
            if (type == Type::End || type == Type::EndAlternate) {
                return vdf::Node::from(std::move(entries));
            }
            std::string key = keyOf(in);
            entries.emplace_back(std::move(key), value(in, type, depth));
        }
    }

  private:
    [[nodiscard]] std::string keyOf(Cursor& in) const {
        if (keys_ == nullptr) {
            return in.text();
        }
        const auto index = in.fixed<std::int32_t>();
        if (index < 0 || static_cast<std::size_t>(index) >= keys_->size()) {
            throw Malformed{"a key is outside the string table"};
        }
        return (*keys_)[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] vdf::Node::Value value(Cursor& in, Type type, int depth) const {
        switch (type) {
        case Type::Block:
            return block(in, depth + 1);
        case Type::String:
            return in.text();
        case Type::Int32:
        case Type::Pointer:
        case Type::Color:
            return std::to_string(in.fixed<std::int32_t>());
        case Type::Float:
            return std::to_string(in.fixed<float>());
        case Type::UInt64:
            return std::to_string(in.fixed<std::uint64_t>());
        case Type::Int64:
            return std::to_string(in.fixed<std::int64_t>());
        case Type::End:
        case Type::EndAlternate:
            break;
        }
        throw Malformed{"unknown value type " + std::to_string(static_cast<int>(type))};
    }

    const std::vector<std::string>* keys_;
};

std::vector<std::string> stringTable(std::string_view data, std::size_t at) {
    Cursor in{data, at};
    const auto count = in.fixed<std::uint32_t>();
    if (count > data.size()) {
        throw Malformed{"the string table is larger than the file"};
    }
    std::vector<std::string> keys;
    keys.reserve(count);
    for (std::uint32_t n = 0; n < count; ++n) {
        keys.push_back(in.text());
    }
    return keys;
}

std::map<std::string, vdf::Node, std::less<>>
decode(std::string_view data, const std::set<std::string, std::less<>>& ids) {
    Cursor header{data, 0};
    const auto magic = header.fixed<std::uint32_t>();
    static_cast<void>(header.fixed<std::uint32_t>());
    std::optional<std::vector<std::string>> keys;
    std::size_t at = 2 * sizeof(std::uint32_t);
    if (magic == tableKeys) {
        const auto table = header.fixed<std::int64_t>();
        if (table < 0) {
            throw Malformed{"the string table offset is negative"};
        }
        keys = stringTable(data, static_cast<std::size_t>(table));
        at += sizeof(std::int64_t);
    } else if (magic != inlineKeys) {
        throw Malformed{"format " + std::to_string(magic & 0xFFU) + " is not read"};
    }
    const Decoder decoder{keys ? &*keys : nullptr};
    std::map<std::string, vdf::Node, std::less<>> found;
    while (found.size() < ids.size()) {
        Cursor app{data, at};
        const auto appId = app.fixed<std::uint32_t>();
        if (appId == 0) {
            break;
        }
        const auto size = app.fixed<std::uint32_t>();
        const std::size_t next = at + 2 * sizeof(std::uint32_t) + size;
        if (const std::string id = std::to_string(appId); ids.contains(id)) {
            Cursor body{data, at + 2 * sizeof(std::uint32_t) + appPrefix};
            vdf::Node root = decoder.block(body, 0);
            std::optional<vdf::Node> info = root.block({"appinfo"});
            found.emplace(id, info ? std::move(*info) : std::move(root));
        }
        at = next;
    }
    return found;
}

} // namespace

std::map<std::string, vdf::Node, std::less<>>
readAppInfo(const std::filesystem::path& file, const std::set<std::string, std::less<>>& appIds) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        return {};
    }
    const std::string data{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    try {
        return decode(data, appIds);
    } catch (const Malformed& error) {
        lucent::warn("steam", "{} not read: {}", file.string(), error.what());
        return {};
    }
}

} // namespace opensu::library::steam

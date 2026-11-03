#include "lutris_config.hpp"

#include <fstream>
#include <vector>

namespace opensu::library::lutris {
namespace {

namespace fs = std::filesystem;

std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r");
    text = text.substr(first, last - first + 1);
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') &&
        text.back() == text.front()) {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

/// One mapping open above the line being read.
struct Open {
    std::size_t indent{0};
    std::string key;
};

/// Whether the open mappings and then `key` are `keys`.
bool at(const std::vector<Open>& open, std::string_view key,
        std::initializer_list<std::string_view> keys) {
    if (open.size() + 1 != keys.size()) {
        return false;
    }
    const auto* wanted = keys.begin();
    for (const Open& mapping : open) {
        if (mapping.key != *wanted) {
            return false;
        }
        ++wanted;
    }
    return key == *wanted;
}

} // namespace

std::string configValue(const fs::path& file, std::initializer_list<std::string_view> keys) {
    std::ifstream in{file};
    std::vector<Open> open;
    for (std::string line; std::getline(in, line);) {
        const std::string_view text{line};
        const std::size_t indent = text.find_first_not_of(' ');
        if (indent == std::string_view::npos || text[indent] == '#' || text[indent] == '-') {
            continue;
        }
        const std::size_t colon = text.find(':', indent);
        if (colon == std::string_view::npos) {
            continue;
        }
        while (!open.empty() && open.back().indent >= indent) {
            open.pop_back();
        }
        const std::string_view key = trimmed(text.substr(indent, colon - indent));
        const std::string_view value = trimmed(text.substr(colon + 1));
        if (value.empty()) {
            open.push_back(Open{indent, std::string{key}});
        } else if (at(open, key, keys)) {
            return std::string{value};
        }
    }
    return {};
}

fs::path gameConfig(const fs::path& lutrisData, std::string_view configpath) {
    return lutrisData / "games" / (std::string{configpath} + ".yml");
}

} // namespace opensu::library::lutris

#include "lutris_config.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

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

/// One `key: value` line and the mappings above it.
struct Leaf {
    std::vector<std::string> mappings;
    std::string key;
    std::string value;
};

std::vector<Leaf> leavesOf(const fs::path& file) {
    std::ifstream in{file};
    std::vector<Open> open;
    std::vector<Leaf> leaves;
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
            continue;
        }
        std::vector<std::string> mappings;
        mappings.reserve(open.size());
        for (const Open& mapping : open) {
            mappings.push_back(mapping.key);
        }
        leaves.push_back(Leaf{std::move(mappings), std::string{key}, std::string{value}});
    }
    return leaves;
}

/// Whether `mappings` are the first `count` of `keys` and nothing more.
bool under(const std::vector<std::string>& mappings, std::size_t count,
           std::initializer_list<std::string_view> keys) {
    return mappings.size() == count && std::equal(mappings.begin(), mappings.end(), keys.begin());
}

} // namespace

std::string configValue(const fs::path& file, std::initializer_list<std::string_view> keys) {
    for (Leaf& leaf : leavesOf(file)) {
        if (under(leaf.mappings, keys.size() - 1, keys) && leaf.key == *std::prev(keys.end())) {
            return std::move(leaf.value);
        }
    }
    return {};
}

std::vector<std::pair<std::string, std::string>>
configMapping(const fs::path& file, std::initializer_list<std::string_view> keys) {
    std::vector<std::pair<std::string, std::string>> found;
    for (Leaf& leaf : leavesOf(file)) {
        if (under(leaf.mappings, keys.size(), keys)) {
            found.emplace_back(std::move(leaf.key), std::move(leaf.value));
        }
    }
    return found;
}

fs::path gameConfig(const fs::path& lutrisData, std::string_view configpath) {
    return lutrisData / "games" / (std::string{configpath} + ".yml");
}

} // namespace opensu::library::lutris

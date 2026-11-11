#include "ini_file.hpp"

#include <algorithm>

#include "fileio/read_whole.hpp"

namespace opensu::library {
namespace {

constexpr std::string_view blank = " \t\r";

std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(blank);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blank) - first + 1);
}

bool isSection(std::string_view line, std::string_view section) {
    return line.size() == section.size() + 2 && line.starts_with("[") && line.ends_with("]") &&
           line.substr(1, section.size()) == section;
}

/// Where `key`'s first line in `[section]` is, or where the section's settings end.
struct Found {
    /// The key's line, without its line break; npos when the key is not there.
    std::size_t line{std::string_view::npos};
    std::size_t lineEnd{std::string_view::npos};
    /// Just past the line break of the section's last setting; npos when the section is not there.
    std::size_t sectionEnd{std::string_view::npos};
};

Found find(std::string_view text, const IniKey& wanted) {
    const std::string_view section = wanted.section;
    Found found;
    bool inSection = false;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end;
        const std::string_view line = trimmed(text.substr(at, end - at));
        const std::size_t next = end == text.size() ? end : end + 1;
        if (line.starts_with("[")) {
            inSection = isSection(line, section);
            if (inSection) {
                found.sectionEnd = next;
            }
        } else if (inSection) {
            const std::size_t equals = line.find_first_of("=:");
            if (equals != std::string_view::npos && trimmed(line.substr(0, equals)) == wanted.key) {
                found.line = at;
                found.lineEnd = end;
                return found;
            }
            if (!line.empty()) {
                found.sectionEnd = next;
            }
        }
        at = next;
    }
    return found;
}

} // namespace

std::optional<std::string> iniValueIn(std::string_view text, const IniKey& key) {
    const Found found = find(text, key);
    if (found.line == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view line = trimmed(text.substr(found.line, found.lineEnd - found.line));
    return std::string{trimmed(line.substr(line.find_first_of("=:") + 1))};
}

std::optional<std::string> iniValue(const std::filesystem::path& file, std::string_view section,
                                    std::string_view key) {
    std::string error;
    const std::optional<std::string> text = fileio::readWhole(file, error);
    if (!text) {
        return std::nullopt;
    }
    return iniValueIn(*text, IniKey{.section = section, .key = key});
}

std::vector<std::string> iniKeysIn(std::string_view text, std::string_view section) {
    std::vector<std::string> keys;
    bool inSection = false;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end;
        const std::string_view line = trimmed(text.substr(at, end - at));
        at = end == text.size() ? end : end + 1;
        if (line.starts_with("[")) {
            inSection = isSection(line, section);
            continue;
        }
        const std::size_t equals = line.find_first_of("=:");
        if (!inSection || equals == std::string_view::npos) {
            continue;
        }
        std::string key{trimmed(line.substr(0, equals))};
        if (std::ranges::find(keys, key) == keys.end()) {
            keys.push_back(std::move(key));
        }
    }
    return keys;
}

std::string withIniValue(std::string_view text, const IniKey& key, std::string_view value) {
    const Found found = find(text, key);
    std::string setting{key.key};
    setting += '=';
    setting += value;
    std::string out;
    if (found.line != std::string_view::npos) {
        if (iniValueIn(text, key) == value) {
            return std::string{text};
        }
        out.append(text.substr(0, found.line));
        out.append(setting);
        out.append(text.substr(found.lineEnd));
        return out;
    }
    if (found.sectionEnd != std::string_view::npos) {
        out.append(text.substr(0, found.sectionEnd));
        if (!out.empty() && out.back() != '\n') {
            out += '\n';
        }
        out.append(setting);
        out += '\n';
        out.append(text.substr(found.sectionEnd));
        return out;
    }
    out.append(text);
    if (!out.empty() && out.back() != '\n') {
        out += '\n';
    }
    out += '[';
    out.append(key.section);
    out += "]\n";
    out.append(setting);
    out += '\n';
    return out;
}

std::string withoutIniValue(std::string_view text, const IniKey& key) {
    const Found found = find(text, key);
    if (found.line == std::string_view::npos) {
        return std::string{text};
    }
    std::string out{text.substr(0, found.line)};
    if (found.lineEnd < text.size()) {
        out.append(text.substr(found.lineEnd + 1));
    }
    return out;
}

} // namespace opensu::library

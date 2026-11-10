#include "ini_file.hpp"

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

/// Where `key`'s first line in `[section]` is, or where the section's settings end.
struct Found {
    /// The key's line, without its line break; npos when the key is not there.
    std::size_t line{std::string_view::npos};
    std::size_t lineEnd{std::string_view::npos};
    /// Just past the line break of the section's last setting; npos when the section is not there.
    std::size_t sectionEnd{std::string_view::npos};
};

Found find(std::string_view text, std::string_view section, std::string_view key) {
    Found found;
    bool inSection = false;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end;
        const std::string_view line = trimmed(text.substr(at, end - at));
        const std::size_t next = end == text.size() ? end : end + 1;
        if (line.starts_with("[")) {
            inSection = line.size() == section.size() + 2 && line.ends_with("]") &&
                        line.substr(1, section.size()) == section;
            if (inSection) {
                found.sectionEnd = next;
            }
        } else if (inSection) {
            const std::size_t equals = line.find_first_of("=:");
            if (equals != std::string_view::npos && trimmed(line.substr(0, equals)) == key) {
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

std::optional<std::string> iniValueIn(std::string_view text, std::string_view section,
                                      std::string_view key) {
    const Found found = find(text, section, key);
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
    return iniValueIn(*text, section, key);
}

std::string withIniValue(std::string_view text, std::string_view section, std::string_view key,
                         std::string_view value) {
    const Found found = find(text, section, key);
    std::string setting{key};
    setting += '=';
    setting += value;
    std::string out;
    if (found.line != std::string_view::npos) {
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
    out.append(section);
    out += "]\n";
    out.append(setting);
    out += '\n';
    return out;
}

} // namespace opensu::library

#include "steam_shortcuts.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "vdf/binary.hpp"

namespace opensu::library::own {
namespace {

namespace fs = std::filesystem;

/// Steam keeps the program and its folder in double quotes.
std::string unquoted(std::string text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

/// The app id as Steam shows it: the file keeps it as a signed 32-bit integer.
std::string appIdOf(const std::string& stored) {
    std::int64_t value = 0;
    const char* end = stored.data() + stored.size();
    if (std::from_chars(stored.data(), end, value).ptr != end) {
        return stored;
    }
    return std::to_string(static_cast<std::uint32_t>(value));
}

std::optional<Entry> entryOf(const vdf::Node& shortcut) {
    const std::string exe = unquoted(shortcut.str({"Exe"}).value_or(""));
    const std::string name = shortcut.str({"AppName"}).value_or("");
    if (exe.empty() || name.empty()) {
        return std::nullopt;
    }
    return Entry{.id = {},
                 .title = name,
                 .file = exe,
                 .args = launchWords(shortcut.str({"LaunchOptions"}).value_or("")),
                 .workingDirectory = unquoted(shortcut.str({"StartDir"}).value_or("")),
                 .origin = Origin::SteamShortcut,
                 .originId = appIdOf(shortcut.str({"appid"}).value_or(""))};
}

} // namespace

std::vector<std::string> launchWords(std::string_view options) {
    std::vector<std::string> words;
    std::string word;
    bool quoted = false;
    bool inWord = false;
    for (const char c : options) {
        if (c == '"') {
            quoted = !quoted;
            inWord = true;
        } else if (!quoted && (c == ' ' || c == '\t')) {
            if (inWord) {
                words.push_back(std::move(word));
                word.clear();
                inWord = false;
            }
        } else {
            word += c;
            inWord = true;
        }
    }
    if (inWord) {
        words.push_back(std::move(word));
    }
    if (const auto command = std::ranges::find(words, "%command%"); command != words.end()) {
        words.erase(words.begin(), command + 1);
    }
    return words;
}

std::vector<Entry> steamShortcuts(const std::vector<fs::path>& steamRoots) {
    std::vector<Entry> entries;
    for (const fs::path& root : steamRoots) {
        std::error_code error;
        for (const fs::directory_entry& user : fs::directory_iterator{root / "userdata", error}) {
            const std::optional<vdf::Node> file =
                vdf::parseBinaryFile(user.path() / "config" / "shortcuts.vdf");
            const std::optional<vdf::Node> shortcuts =
                file ? file->block({"shortcuts"}) : std::nullopt;
            if (!shortcuts) {
                continue;
            }
            for (const std::string& key : shortcuts->keys()) {
                const std::optional<vdf::Node> shortcut = shortcuts->block({key});
                std::optional<Entry> entry = shortcut ? entryOf(*shortcut) : std::nullopt;
                if (entry && std::ranges::find(entries, entry->originId, &Entry::originId) ==
                                 entries.end()) {
                    entries.push_back(std::move(*entry));
                }
            }
        }
    }
    return entries;
}

} // namespace opensu::library::own

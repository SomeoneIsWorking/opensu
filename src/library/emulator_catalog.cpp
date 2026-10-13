#include "emulator_catalog.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <regex>
#include <tuple>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

/// Thrown for a malformed field, carrying where it is.
class Malformed : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

std::string text(const json& object, const char* key, const std::string& where, bool required) {
    const auto found = object.find(key);
    if (found == object.end()) {
        if (required) {
            throw Malformed{where + ": no \"" + key + "\""};
        }
        return {};
    }
    if (!found->is_string()) {
        throw Malformed{where + ": \"" + key + "\" is not a string"};
    }
    return found->get<std::string>();
}

std::vector<std::string> words(const json& object, const char* key, const std::string& where) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return {};
    }
    if (!found->is_array() || !std::ranges::all_of(*found, [](const json& word) {
            return word.is_string();
        })) {
        throw Malformed{where + ": \"" + key + "\" is not a list of strings"};
    }
    return found->get<std::vector<std::string>>();
}

fs::path configPath(const std::string& raw, const fs::path& home) {
    if (raw.starts_with("~/")) {
        return home / raw.substr(2);
    }
    return fs::path{raw};
}

ReleaseSource releaseOf(const json& object, const std::string& where) {
    ReleaseSource release{text(object, "api", where, true), text(object, "asset", where, true),
                          text(object, "downloads", where, true)};
    try {
        std::ignore = std::regex{release.asset};
    } catch (const std::regex_error&) {
        throw Malformed{where + ": \"asset\" is not a regular expression"};
    }
    if (release.downloads.find("://") == std::string::npos || !release.downloads.ends_with("/")) {
        throw Malformed{where + ": \"downloads\" must be an origin and path ending in /"};
    }
    return release;
}

EmulatorEntry entryOf(const json& object, const fs::path& home, std::size_t index) {
    std::string where = "emulator " + std::to_string(index);
    if (!object.is_object()) {
        throw Malformed{where + " is not an object"};
    }
    EmulatorEntry entry;
    entry.id = text(object, "id", where, true);
    where = entry.id;
    entry.name = text(object, "name", where, true);
    entry.systems = words(object, "systems", where);
    if (entry.systems.empty()) {
        throw Malformed{where + ": no systems"};
    }
    entry.config = configPath(text(object, "config", where, false), home);
    const auto launchers = object.find("launchers");
    if (launchers == object.end() || !launchers->is_array() || launchers->empty()) {
        throw Malformed{where + ": no launchers"};
    }
    for (const json& launcher : *launchers) {
        if (!launcher.is_object()) {
            throw Malformed{where + ": a launcher is not an object"};
        }
        entry.launchers.push_back(EmulatorLauncher{
            words(launcher, "programs", where), text(launcher, "appImage", where, false),
            text(launcher, "flatpak", where, false), words(launcher, "args", where)});
    }
    if (const auto release = object.find("release"); release != object.end()) {
        if (!release->is_object()) {
            throw Malformed{where + ": \"release\" is not an object"};
        }
        entry.release = releaseOf(*release, where);
    }
    return entry;
}

} // namespace

bool EmulatorEntry::serves(std::string_view system) const {
    return std::ranges::find(systems, system) != systems.end();
}

std::optional<EmulatorCatalog> EmulatorCatalog::parse(std::string_view text, const fs::path& home,
                                                      std::string& error) {
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        error = "not a JSON object";
        return std::nullopt;
    }
    const auto list = root.find("emulators");
    if (list == root.end() || !list->is_array()) {
        error = "no \"emulators\" list";
        return std::nullopt;
    }
    std::vector<EmulatorEntry> entries;
    try {
        for (const json& object : *list) {
            EmulatorEntry entry = entryOf(object, home, entries.size());
            if (std::ranges::find(entries, entry.id, &EmulatorEntry::id) != entries.end()) {
                throw Malformed{entry.id + ": listed twice"};
            }
            entries.push_back(std::move(entry));
        }
    } catch (const Malformed& malformed) {
        error = malformed.what();
        return std::nullopt;
    }
    return EmulatorCatalog{std::move(entries)};
}

EmulatorCatalog EmulatorCatalog::shipped(const fs::path& assetsDir, const fs::path& home) {
    const fs::path file = assetsDir / catalogFile;
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::error("emulators", "{}: cannot be read; no emulator is known", file.string());
        return {};
    }
    const std::string body{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string error;
    std::optional<EmulatorCatalog> catalog = parse(body, home, error);
    if (!catalog) {
        lucent::error("emulators", "{}: {}; no emulator is known", file.string(), error);
        return {};
    }
    return std::move(*catalog);
}

const EmulatorEntry* EmulatorCatalog::find(std::string_view id) const {
    const auto found = std::ranges::find(entries_, id, &EmulatorEntry::id);
    return found == entries_.end() ? nullptr : &*found;
}

const EmulatorEntry* EmulatorCatalog::findByName(std::string_view name) const {
    const auto found = std::ranges::find(entries_, name, &EmulatorEntry::name);
    return found == entries_.end() ? nullptr : &*found;
}

} // namespace opensu::library::roms

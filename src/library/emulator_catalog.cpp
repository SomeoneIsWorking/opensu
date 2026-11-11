#include "emulator_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <regex>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"
#include "rom_systems.hpp"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view coreSuffix = "_libretro";

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

/// `path` moved from under `from` to under `to`; as it is when it is not under `from`.
fs::path rebased(const fs::path& path, const fs::path& from, const fs::path& to) {
    const fs::path rest = path.lexically_relative(from);
    if (path.empty() || rest.empty() || *rest.begin() == "..") {
        return path;
    }
    return rest == "." ? to : to / rest;
}

fs::path configPath(const std::string& raw, const fs::path& home) {
    if (raw.starts_with("~/")) {
        return home / raw.substr(2);
    }
    return fs::path{raw};
}

void checkPattern(const std::string& pattern, const std::string& where,
                  std::string_view key = "asset") {
    try {
        std::ignore = std::regex{pattern};
    } catch (const std::regex_error&) {
        throw Malformed{std::format("{}: \"{}\" is not a regular expression", where, key)};
    }
}

ReleaseSource releaseOf(const json& object, const std::string& where, std::string asset) {
    ReleaseSource release{text(object, "api", where, true), std::move(asset),
                          text(object, "downloads", where, true)};
    if (release.downloads.find("://") == std::string::npos || !release.downloads.ends_with("/")) {
        throw Malformed{where + ": \"downloads\" must be an origin and path ending in /"};
    }
    return release;
}

ReleaseSource releaseOf(const json& object, const std::string& where) {
    std::string asset = text(object, "asset", where, true);
    checkPattern(asset, where);
    ReleaseSource release = releaseOf(object, where, std::move(asset));
    release.member = text(object, "member", where, false);
    if (!release.member.empty()) {
        checkPattern(release.member, where, "member");
    }
    return release;
}

bool isSha256(const json& value) {
    constexpr std::size_t hexDigits = 64;
    return value.is_string() && value.get_ref<const std::string&>().size() == hexDigits &&
           std::ranges::all_of(value.get_ref<const std::string&>(), [](unsigned char c) {
               return std::isdigit(c) != 0 || (c >= 'a' && c <= 'f');
           });
}

/// A core's `"release"`: its asset for each build, as `{"android-arm64-v8a": pattern}`, and the
/// SHA-256 each is pinned to when it names `"sha256"` by the same builds. Nothing on a build it
/// names no asset for, which takes the buildbot's core.
std::optional<ReleaseSource> coreReleaseOf(const json& object, std::string_view build,
                                           const std::string& where) {
    const auto found = object.find("release");
    if (found == object.end()) {
        return std::nullopt;
    }
    const auto assets = found->is_object() ? found->find("assets") : found->end();
    if (assets == found->end() || !assets->is_object() || assets->empty() ||
        found->contains("asset")) {
        throw Malformed{where + ": a core's \"release\" names its \"assets\" by build"};
    }
    const auto pins = found->find("sha256");
    if (pins != found->end() && (!pins->is_object() || pins->size() != assets->size())) {
        throw Malformed{where + ": a core's \"sha256\" pins every asset by build"};
    }
    std::optional<ReleaseSource> chosen;
    for (const auto& [name, pattern] : assets->items()) {
        if (!pattern.is_string()) {
            throw Malformed{std::format("{}: the asset of {} is not a string", where, name)};
        }
        checkPattern(pattern.get<std::string>(), where);
        ReleaseSource release = releaseOf(*found, where, pattern.get<std::string>());
        if (pins != found->end()) {
            const auto pin = pins->find(name);
            if (pin == pins->end() || !isSha256(*pin)) {
                throw Malformed{std::format("{}: the SHA-256 of {} is not 64 lower-case hex digits",
                                            where, name)};
            }
            release.sha256 = pin->get<std::string>();
        }
        if (name == build) {
            chosen = std::move(release);
        }
    }
    return chosen;
}

/// A core's `"links"`: systems it serves on which it links two players.
std::vector<std::string> linksOf(const json& object, const std::vector<std::string>& systems,
                                 const std::string& where) {
    std::vector<std::string> links = words(object, "links", where);
    if (!links.empty() && !object.contains("release")) {
        throw Malformed{where + ": only a core from its own \"release\" links players"};
    }
    for (const std::string& system : links) {
        if (std::ranges::find(systems, system) == systems.end()) {
            throw Malformed{std::format("{}: it links {}, which it does not run", where, system)};
        }
    }
    return links;
}

/// A standalone emulator's `"saves"`: its layout and folder.
std::optional<SaveStore> savesOf(const json& object, const fs::path& home,
                                 const std::string& where) {
    const auto found = object.find("saves");
    if (found == object.end()) {
        return std::nullopt;
    }
    if (!found->is_object()) {
        throw Malformed{where + ": \"saves\" is not an object"};
    }
    constexpr std::array layouts{std::pair{"yuzu", SaveLayout::YuzuNand},
                                 std::pair{"ryujinx", SaveLayout::RyujinxBis},
                                 std::pair{"mgba", SaveLayout::Mgba},
                                 std::pair{"duckstation", SaveLayout::DuckStationCards}};
    const std::string layout = text(*found, "layout", where, true);
    const auto known = std::ranges::find_if(layouts, [&layout](const auto& entry) {
        return layout == entry.first;
    });
    if (known == layouts.end()) {
        throw Malformed{where +
                        ": \"saves\" \"layout\" must be yuzu, ryujinx, mgba or duckstation"};
    }
    // mGBA's saves are beside each ROM unless its settings name a folder.
    const bool folderNamed = known->second != SaveLayout::Mgba;
    return SaveStore{.layout = known->second,
                     .folder = configPath(text(*found, "folder", where, folderNamed), home),
                     .flatpakFolder = {}};
}

/// An arcade core's listings: HTTPS files, or a release.
RomsetSource romsetsOf(const json& object, const std::string& where) {
    const auto found = object.find("romsets");
    if (found == object.end()) {
        return {};
    }
    if (!found->is_object()) {
        throw Malformed{where + ": \"romsets\" is not an object"};
    }
    RomsetSource source{words(*found, "files", where), std::nullopt};
    if (const auto release = found->find("release"); release != found->end()) {
        if (!release->is_object()) {
            throw Malformed{where + ": \"romsets\" \"release\" is not an object"};
        }
        source.release = releaseOf(*release, where);
    }
    if (source.empty() || (!source.files.empty() && source.release)) {
        throw Malformed{where + ": \"romsets\" needs \"files\" or a \"release\""};
    }
    for (const std::string& file : source.files) {
        if (!file.starts_with("https://")) {
            throw Malformed{where + ": a romset listing must be fetched over HTTPS"};
        }
    }
    return source;
}

/// A built-in core's `"coreOptions"`: option key to value, both strings.
std::vector<CoreOption> coreOptionsOf(const json& object, const std::string& where) {
    const auto found = object.find("coreOptions");
    if (found == object.end()) {
        return {};
    }
    if (!found->is_object()) {
        throw Malformed{where + ": \"coreOptions\" is not an object"};
    }
    std::vector<CoreOption> options;
    for (const auto& [key, value] : found->items()) {
        if (key.empty() || key.find('=') != std::string::npos || !value.is_string()) {
            throw Malformed{where + ": \"coreOptions\" maps option keys to string values"};
        }
        options.push_back({key, value.get<std::string>()});
    }
    return options;
}

/// A built-in core's `"systemFiles"`: `url` (HTTPS), `sha256`, the zip's `folder` and where it
/// goes, `into`.
std::optional<SystemFilesSource> systemFilesOf(const json& object, const std::string& where) {
    const auto found = object.find("systemFiles");
    if (found == object.end()) {
        return std::nullopt;
    }
    if (!found->is_object()) {
        throw Malformed{where + ": \"systemFiles\" is not an object"};
    }
    SystemFilesSource source{text(*found, "url", where, true),
                             {},
                             text(*found, "folder", where, true),
                             text(*found, "into", where, true)};
    if (!source.url.starts_with("https://")) {
        throw Malformed{where + ": \"systemFiles\" must be fetched over HTTPS"};
    }
    const auto pin = found->find("sha256");
    if (pin == found->end() || !isSha256(*pin)) {
        throw Malformed{where + ": \"systemFiles\" pins its zip by 64 lower-case hex digits"};
    }
    source.sha256 = pin->get<std::string>();
    if (!isInsidePath(source.folder) || !isInsidePath(source.into)) {
        throw Malformed{where + ": \"systemFiles\" \"folder\" and \"into\" are relative paths"};
    }
    return source;
}

/// A built-in emulator's core: a file name of the buildbot's, and nothing else to start it with.
void checkCore(const json& object, const std::string& core, const std::string& where) {
    const bool named = core.ends_with(coreSuffix) && core.size() > coreSuffix.size() &&
                       std::ranges::all_of(core, [](unsigned char c) {
                           return std::islower(c) != 0 || std::isdigit(c) != 0 || c == '_';
                       });
    if (!named) {
        throw Malformed{where + ": \"core\" must be a core's name, as \"mgba_libretro\""};
    }
    if (object.contains("launchers") || object.contains("saves")) {
        throw Malformed{where + ": a core runs built in, without \"launchers\" or "
                                "\"saves\""};
    }
}

EmulatorEntry entryOf(const json& object, const fs::path& home, std::string_view build,
                      const fs::path& xdgConfigHome, std::size_t index) {
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
    entry.data = configPath(text(object, "data", where, false), home);
    entry.core = text(object, "core", where, false);
    if (entry.builtIn()) {
        checkCore(object, entry.core, where);
        entry.release = coreReleaseOf(object, build, where);
        entry.romsets = romsetsOf(object, where);
        entry.coreOptions = coreOptionsOf(object, where);
        entry.systemFiles = systemFilesOf(object, where);
        entry.links = linksOf(object, entry.systems, where);
        const std::string without = text(object, "withoutBios", where, false);
        const std::optional<WithoutBios> mode =
            without.empty() ? WithoutBios::Needed : withoutBiosNamed(without);
        if (!mode) {
            throw Malformed{where + ": \"withoutBios\" must be needed, hle, open or skipped"};
        }
        entry.withoutBios = *mode;
        const std::string scan = text(object, "biosScan", where, false);
        const std::optional<BiosScan> rule = scan.empty() ? BiosScan::Listed : biosScanNamed(scan);
        if (!rule) {
            throw Malformed{where + ": \"biosScan\" must be pcsx_rearmed or lrps2"};
        }
        entry.biosScan = *rule;
        return entry;
    }
    if (object.contains("withoutBios") || object.contains("biosScan") ||
        object.contains("coreOptions") || object.contains("links") ||
        object.contains("systemFiles")) {
        throw Malformed{where + ": \"withoutBios\", \"biosScan\", \"coreOptions\", \"links\" and "
                                "\"systemFiles\" are for a built-in emulator's core"};
    }
    const auto launchers = object.find("launchers");
    if (launchers == object.end() || !launchers->is_array() || launchers->empty()) {
        throw Malformed{where + ": no launchers and no core"};
    }
    for (const json& launcher : *launchers) {
        if (!launcher.is_object()) {
            throw Malformed{where + ": a launcher is not an object"};
        }
        entry.launchers.push_back(EmulatorLauncher{
            words(launcher, "programs", where), text(launcher, "appImage", where, false),
            text(launcher, "flatpak", where, false), text(launcher, "android", where, false),
            words(launcher, "args", where)});
    }
    if (const auto release = object.find("release"); release != object.end()) {
        if (!release->is_object()) {
            throw Malformed{where + ": \"release\" is not an object"};
        }
        entry.release = releaseOf(*release, where);
    }
    entry.saves = savesOf(object, home, where);
    const auto flatpak =
        std::ranges::find_if(entry.launchers, [](const EmulatorLauncher& launcher) {
            return !launcher.flatpak.empty();
        });
    if (flatpak != entry.launchers.end()) {
        entry.flatpakConfig = flatpakFolder(entry.config, home, flatpak->flatpak);
        entry.flatpakData = flatpakFolder(entry.data, home, flatpak->flatpak);
        if (entry.saves) {
            entry.saves->flatpakFolder = flatpakFolder(entry.saves->folder, home, flatpak->flatpak);
        }
    }
    if (const std::string xdg = text(object, "xdgConfig", where, false); !xdg.empty()) {
        // A Flatpak's sandbox always sets XDG_CONFIG_HOME, to its own config folder.
        if (flatpak != entry.launchers.end()) {
            const fs::path inside =
                home / ".var" / "app" / flatpak->flatpak / "config" / fs::path{xdg};
            if (entry.saves) {
                entry.saves->flatpakFolder = rebased(entry.saves->folder, entry.config, inside);
            }
            entry.flatpakConfig = inside;
        }
        if (!xdgConfigHome.empty()) {
            const fs::path chosen = xdgConfigHome / fs::path{xdg};
            if (entry.saves) {
                entry.saves->folder = rebased(entry.saves->folder, entry.config, chosen);
            }
            entry.config = chosen;
        }
    }
    return entry;
}

} // namespace

bool isInsidePath(std::string_view path) {
    const fs::path relative = fs::path{path}.lexically_normal();
    return !path.empty() && relative.is_relative() && relative.generic_string() == path &&
           *relative.begin() != "..";
}

fs::path flatpakFolder(const fs::path& path, const fs::path& home, std::string_view appId) {
    const fs::path sandbox = home / ".var" / "app" / std::string{appId};
    for (const auto& [host, inside] :
         {std::pair{home / ".config", "config"}, std::pair{home / ".local" / "share", "data"}}) {
        const fs::path rest = path.lexically_relative(host);
        if (!path.empty() && !rest.empty() && *rest.begin() != "..") {
            return sandbox / inside / rest;
        }
    }
    return {};
}

std::string_view EmulatorEntry::shortName() const noexcept {
    std::string_view full{name};
    if (full.ends_with(builtInMark)) {
        full.remove_suffix(builtInMark.size());
    }
    return full;
}

bool EmulatorEntry::serves(std::string_view system) const {
    return std::ranges::find(systems, system) != systems.end();
}

std::optional<EmulatorCatalog> EmulatorCatalog::parse(std::string_view text, const fs::path& home,
                                                      std::string& error, std::string_view build,
                                                      const fs::path& xdgConfigHome) {
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
            EmulatorEntry entry = entryOf(object, home, build, xdgConfigHome, entries.size());
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

EmulatorCatalog EmulatorCatalog::shipped(const fs::path& assetsDir, const fs::path& home,
                                         const fs::path& xdgConfigHome) {
    const fs::path file = assetsDir / catalogFile;
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::error("emulators", "{}: cannot be read; no emulator is known", file.string());
        return {};
    }
    const std::string body{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string error;
    std::optional<EmulatorCatalog> catalog =
        parse(body, home, error, coreTarget.build, xdgConfigHome);
    if (!catalog) {
        lucent::error("emulators", "{}: {}; no emulator is known", file.string(), error);
        return {};
    }
    return std::move(*catalog);
}

EmulatorCatalog EmulatorCatalog::builtInOnly() const {
    std::vector<EmulatorEntry> kept;
    std::ranges::copy_if(entries_, std::back_inserter(kept), &EmulatorEntry::builtIn);
    return EmulatorCatalog{std::move(kept)};
}

std::vector<std::string> EmulatorCatalog::servedSystems() const {
    std::vector<std::string> systems;
    for (const RomSystem& system : romSystems()) {
        const bool served = std::ranges::any_of(entries_, [&system](const EmulatorEntry& entry) {
            return entry.serves(system.key);
        });
        if (served) {
            systems.emplace_back(system.key);
        }
    }
    return systems;
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

#include "settings.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::settings {
namespace {

using json = nlohmann::json;

constexpr const char* libraryModeKey = "libraryMode";
constexpr const char* pinLibraryDockKey = "pinLibraryDock";
constexpr const char* iconSizeKey = "iconSize";
constexpr const char* sortKey = "sort";
constexpr const char* installedOnlyKey = "installedOnly";
constexpr const char* sourceKey = "source";
constexpr const char* hiddenKey = "hidden";
constexpr const char* lastPlayedKey = "lastPlayed";
constexpr const char* emulatorsKey = "emulators";
constexpr const char* homeModeKey = "homeMode";
constexpr const char* uiSoundsKey = "uiSounds";
constexpr const char* romFoldersKey = "romFolders";
constexpr const char* steamRootsKey = "steamRoots";
constexpr const char* uiScaleKey = "uiScale";
constexpr const char* shortcutsKey = "shortcuts";
constexpr const char* shortcutKeysKey = "keys";
constexpr const char* shortcutPadsKey = "pads";
constexpr const char* installFoldersKey = "installFolders";
constexpr const char* defaultFolderKey = "default";

/// The key a store's own install folder is kept under.
std::string storeKey(library::Source store) {
    std::string key{library::label(store)};
    std::ranges::transform(key, key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return key;
}

/// Reads a file's fields, each on its own: one that is damaged is reported and keeps its default.
class Reader {
  public:
    Reader(const json& document, std::string file) : document_{document}, file_{std::move(file)} {
    }

    void boolean(const char* name, bool& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_boolean()) {
            out = found->get<bool>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void integer(const char* name, int low, int high, int& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_number_integer() && found->get<long long>() >= low &&
            found->get<long long>() <= high) {
            out = found->get<int>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void text(const char* name, std::string& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (found->is_string()) {
            out = found->get<std::string>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void texts(const char* name, std::vector<std::string>& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        const bool strings = found->is_array() && std::ranges::all_of(*found, [](const json& item) {
                                 return item.is_string();
                             });
        if (strings) {
            out = found->get<std::vector<std::string>>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void paths(const char* name, std::vector<std::filesystem::path>& out,
               const char* fallback) const {
        std::vector<std::string> listed;
        texts(name, listed, fallback);
        out.assign(listed.begin(), listed.end());
    }

    void installFolders(const char* name, InstallFolders& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        bool strings = found->is_object();
        for (const auto& item : found->items()) {
            strings = strings && item.value().is_string();
        }
        if (!strings) {
            reject(name, *found, fallback);
            return;
        }
        out.setDefault(found->value(defaultFolderKey, std::string{}));
        for (const library::Source store : installStores) {
            out.setStore(store, found->value(storeKey(store), std::string{}));
        }
    }

    /// The shortcut changes in `name`, each tried against the table so a clash or an unknown key is
    /// reported and dropped.
    void shortcuts(const char* name, input::ShortcutOverrides& out) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (!found->is_object()) {
            reject(name, *found, "using the shipped shortcuts");
            return;
        }
        input::Shortcuts table;
        const json keys = found->value(shortcutKeysKey, json::object());
        const json pads = found->value(shortcutPadsKey, json::object());
        for (const auto& [spelling, entry] : keys.items()) {
            const std::optional<input::Action> action = input::actionOf(spelling);
            if (!entry.is_object()) {
                lucent::warn("settings", "{}: shortcut {} needs a key", file_, spelling);
                continue;
            }
            const input::Combo combo{entry.value("key", 0), entry.value("ctrl", false),
                                     entry.value("shift", false)};
            const std::string refused =
                action ? table.rebind(*action, combo) : "no such action " + spelling;
            if (!refused.empty()) {
                lucent::warn("settings", "{}: shortcut {}: {}", file_, spelling, refused);
            }
        }
        for (const auto& [spelling, entry] : pads.items()) {
            const std::optional<input::Action> action = input::actionOf(spelling);
            const bool pair = entry.is_array() && entry.size() == 2 && entry[0].is_string() &&
                              entry[1].is_string();
            std::string refused = "needs [modifier, trigger]";
            if (action && pair) {
                refused = table.rebind(
                    *action, input::PadChord{gamepad::buttonNamed(entry[0].get<std::string>()),
                                             gamepad::buttonNamed(entry[1].get<std::string>())});
            } else if (!action) {
                refused = "no such action " + spelling;
            }
            if (!refused.empty()) {
                lucent::warn("settings", "{}: shortcut {}: {}", file_, spelling, refused);
            }
        }
        out = table.overrides();
    }

    void times(const char* name, library::PlayHistory::Entries& out, const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        bool numbers = found->is_object();
        for (const auto& item : found->items()) {
            numbers = numbers && item.value().is_number_integer();
        }
        if (numbers) {
            out = found->get<library::PlayHistory::Entries>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void names(const char* name, library::EmulatorChoices::Entries& out,
               const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        bool strings = found->is_object();
        for (const auto& item : found->items()) {
            strings = strings && item.value().is_string();
        }
        if (strings) {
            out = found->get<library::EmulatorChoices::Entries>();
        } else {
            reject(name, *found, fallback);
        }
    }

    void reject(const char* name, const json& value, const char* fallback) const {
        lucent::warn("settings", "{}: {} is not a valid {}; {}", file_, value.dump(), name,
                     fallback);
    }

  private:
    const json& document_;
    std::string file_;
};

} // namespace

Store::Store(std::filesystem::path file) : file_{std::move(file)} {
}

Settings Store::load() const {
    Settings settings;
    std::ifstream in{file_, std::ios::binary};
    if (!in) {
        return settings;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        lucent::warn("settings", "{} is not a settings file; using the defaults", file_.string());
        return settings;
    }
    const Reader read{document, file_.string()};
    read.boolean(pinLibraryDockKey, settings.pinLibraryDock, "the dock stays pinned");
    read.integer(iconSizeKey, minIconSize, maxIconSize, settings.iconSize, "using the default");
    read.boolean(uiSoundsKey, settings.uiSounds, "the sounds play");
    read.paths(romFoldersKey, settings.romFolders, "using the discovered ROM folders");
    read.paths(steamRootsKey, settings.steamRoots, "using the discovered Steam roots");
    read.installFolders(installFoldersKey, settings.installFolders,
                        "every store installs where it chooses");
    read.integer(uiScaleKey, minUiScale, maxUiScale, settings.uiScale, "using 100%");
    read.shortcuts(shortcutsKey, settings.shortcuts);
    read.boolean(installedOnlyKey, settings.view.installedOnly, "showing every game");
    read.text(sourceKey, settings.view.source, "showing every source");
    std::vector<std::string> hidden;
    read.texts(hiddenKey, hidden, "no game is hidden");
    settings.hidden = library::HiddenGames{std::move(hidden)};
    library::PlayHistory::Entries played;
    read.times(lastPlayedKey, played, "no launch is remembered");
    settings.lastPlayed = library::PlayHistory{std::move(played)};
    library::EmulatorChoices::Entries emulators;
    read.names(emulatorsKey, emulators, "every ROM keeps its default emulator");
    settings.emulators = library::EmulatorChoices{std::move(emulators)};

    std::string spelling;
    read.text(sortKey, spelling, "using the default");
    if (!spelling.empty()) {
        if (const std::optional<library::SortKey> sort = library::sortKeyOf(spelling)) {
            settings.view.sort = *sort;
        } else {
            lucent::warn("settings", "{}: {} is not a sort; using the default", file_.string(),
                         spelling);
        }
    }
    std::string homeSpelling;
    read.text(homeModeKey, homeSpelling, "using the environment's");
    if (!homeSpelling.empty()) {
        settings.homeMode = config::homeModeOf(homeSpelling);
        if (!settings.homeMode) {
            lucent::warn("settings", "{}: {} is not a scroll mode; using the environment's",
                         file_.string(), homeSpelling);
        }
    }
    const auto mode = document.find(libraryModeKey);
    if (mode == document.end()) {
        return settings;
    }
    const std::optional<library::LibraryMode> chosen =
        mode->is_string() ? library::libraryModeOf(mode->get<std::string>()) : std::nullopt;
    if (chosen) {
        settings.libraryMode = *chosen;
    } else {
        lucent::warn("settings", "{}: {} is not a Library mode; using Standard", file_.string(),
                     mode->dump());
    }
    return settings;
}

bool Store::save(const Settings& settings, std::string& error) const {
    json folders = json::object();
    if (!settings.installFolders.defaultFolder().empty()) {
        folders[defaultFolderKey] = settings.installFolders.defaultFolder().string();
    }
    for (const library::Source store : installStores) {
        if (const std::filesystem::path own = settings.installFolders.storeFolder(store);
            !own.empty()) {
            folders[storeKey(store)] = own.string();
        }
    }
    const auto listOf = [](const std::vector<std::filesystem::path>& paths) {
        json list = json::array();
        for (const std::filesystem::path& path : paths) {
            list.push_back(path.string());
        }
        return list;
    };
    json shortcutKeys = json::object();
    for (const auto& [action, combo] : settings.shortcuts.keys) {
        shortcutKeys[std::string{input::spelling(action)}] = {
            {"key", combo.key}, {"ctrl", combo.ctrl}, {"shift", combo.shift}};
    }
    json shortcutPads = json::object();
    for (const auto& [action, chord] : settings.shortcuts.pads) {
        shortcutPads[std::string{input::spelling(action)}] = {
            std::string{gamepad::name(chord.modifier)}, std::string{gamepad::name(chord.trigger)}};
    }
    json document{
        {libraryModeKey, std::string{library::key(settings.libraryMode)}},
        {pinLibraryDockKey, settings.pinLibraryDock},
        {iconSizeKey, settings.iconSize},
        {sortKey, std::string{library::key(settings.view.sort)}},
        {installedOnlyKey, settings.view.installedOnly},
        {sourceKey, settings.view.source},
        {hiddenKey, settings.hidden.keys()},
        {lastPlayedKey, settings.lastPlayed.entries()},
        {emulatorsKey, settings.emulators.entries()},
        {uiSoundsKey, settings.uiSounds},
        {romFoldersKey, listOf(settings.romFolders)},
        {steamRootsKey, listOf(settings.steamRoots)},
        {installFoldersKey, folders},
        {uiScaleKey, settings.uiScale},
        {shortcutsKey, {{shortcutKeysKey, shortcutKeys}, {shortcutPadsKey, shortcutPads}}}};
    if (settings.homeMode) {
        document[homeModeKey] = std::string{config::key(*settings.homeMode)};
    }
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

config::Config resolved(config::Config base, const Settings& settings) {
    if (settings.homeMode) {
        base.homeMode = *settings.homeMode;
    }
    if (!settings.romFolders.empty()) {
        base.romRoots = settings.romFolders;
    }
    if (!settings.steamRoots.empty()) {
        base.steamRoots = settings.steamRoots;
    }
    return base;
}

int steppedUiScale(int current, int step) noexcept {
    const int choices = (maxUiScale - minUiScale) / uiScaleStep + 1;
    const int from = std::clamp((current - minUiScale) / uiScaleStep, 0, choices - 1);
    const int to = (from + (step == 0 ? 1 : step) + choices) % choices;
    return minUiScale + to * uiScaleStep;
}

} // namespace opensu::settings

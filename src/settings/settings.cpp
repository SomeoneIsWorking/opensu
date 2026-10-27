#include "settings.hpp"

#include <algorithm>
#include <cmath>
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
constexpr const char* homeModeKey = "homeMode";
constexpr const char* uiSoundsKey = "uiSounds";
constexpr const char* romFoldersKey = "romFolders";
constexpr const char* steamRootsKey = "steamRoots";
constexpr const char* uiScaleKey = "uiScale";
constexpr const char* uiScaleBaseKey = "uiScaleBase";
constexpr const char* keyboardStyleKey = "keyboard";
constexpr const char* shortcutsKey = "shortcuts";
constexpr const char* shortcutKeysKey = "keys";
constexpr const char* shortcutPadsKey = "pads";
constexpr const char* installFoldersKey = "installFolders";
constexpr const char* defaultFolderKey = "default";
constexpr const char* pluginsKey = "plugins";
constexpr const char* enabledKey = "enabled";
constexpr const char* grantedKey = "granted";
constexpr const char* wineRunnerKey = "wineRunner";
constexpr const char* ownWinePrefixKey = "ownWinePrefix";
constexpr const char* wineRunnerPinsKey = "wineRunnerPins";
constexpr const char* keepGeProtonKey = "keepGeProtonCurrent";
constexpr const char* integrationsKey = "integrations";
constexpr const char* steamKey = "steam";
constexpr const char* steamModeKey = "steamMode";
constexpr const char* steamPadsKey = "steamPads";
constexpr const char* epicKey = "epic";
constexpr const char* gogKey = "gog";
constexpr const char* standaloneEmulatorsKey = "emulators";
constexpr const char* luaPluginsKey = "plugins";

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

    void pathsByName(const char* name, std::map<std::string, std::filesystem::path>& out,
                     const char* fallback) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (!found->is_object() || !std::ranges::all_of(*found, [](const json& item) {
                return item.is_string();
            })) {
            reject(name, *found, fallback);
            return;
        }
        for (const auto& item : found->items()) {
            out[item.key()] = item.value().get<std::string>();
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
            out.setStore(store, found->value(library::key(store), std::string{}));
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

    void plugins(const char* name, std::map<std::string, PluginChoice>& out) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        if (!found->is_object()) {
            reject(name, *found, "every plugin keeps its default");
            return;
        }
        for (const auto& [id, entry] : found->items()) {
            const auto enabled = entry.is_object() ? entry.find(enabledKey) : entry.end();
            const auto granted = entry.is_object() ? entry.find(grantedKey) : entry.end();
            bool valid = entry.is_object() && enabled != entry.end() && enabled->is_boolean() &&
                         (granted == entry.end() || granted->is_array());
            if (valid && granted != entry.end()) {
                valid = std::ranges::all_of(*granted, [](const json& item) {
                    return item.is_string();
                });
            }
            if (!valid) {
                reject(name, entry, "the plugin keeps its default");
                continue;
            }
            PluginChoice choice{enabled->get<bool>(), {}};
            if (granted != entry.end()) {
                choice.granted = granted->get<std::vector<std::string>>();
            }
            out[id] = std::move(choice);
        }
    }

    /// The integration choices; a damaged entry is reported and leaves them unchosen, so the
    /// first-run chooser asks again.
    void integrations(const char* name, std::optional<IntegrationChoices>& out) const {
        const auto found = document_.find(name);
        if (found == document_.end()) {
            return;
        }
        const char* fallback = "asking again";
        if (!found->is_object()) {
            reject(name, *found, fallback);
            return;
        }
        IntegrationChoices choices;
        const Reader entry{*found, file_};
        entry.boolean(steamKey, choices.steam, fallback);
        entry.boolean(epicKey, choices.epic, fallback);
        entry.boolean(gogKey, choices.gog, fallback);
        entry.boolean(standaloneEmulatorsKey, choices.emulators, fallback);
        entry.boolean(luaPluginsKey, choices.plugins, fallback);
        std::string mode;
        entry.text(steamModeKey, mode, "using the default Steam mode");
        if (!mode.empty()) {
            choices.steamMode = steamModeOf(mode);
            if (!choices.steamMode) {
                lucent::warn("settings", "{}: {} is not a Steam mode; using the default", file_,
                             mode);
            }
        }
        std::string pads;
        entry.text(steamPadsKey, pads, "using the default Steam controllers");
        if (!pads.empty()) {
            choices.steamPads = steamPadsOf(pads);
            if (!choices.steamPads) {
                lucent::warn("settings",
                             "{}: {} is not a Steam controller choice; using the default", file_,
                             pads);
            }
        }
        out = choices;
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
    // The interface size's base changed: a percent draws at 80% of what it used to, so a size
    // saved before the change is moved up to keep the size the player chose.
    const bool based = document.value(uiScaleBaseKey, 1) >= uiScaleBaseVersion;
    int storedScale = -1;
    read.integer(uiScaleKey, minUiScale, maxUiScale, storedScale, "using 100%");
    if (storedScale >= 0) {
        if (based) {
            settings.uiScale = storedScale;
        } else {
            const float moved = static_cast<float>(storedScale) / uiScaleBase;
            settings.uiScale =
                std::clamp(static_cast<int>(std::lround(moved / static_cast<float>(uiScaleStep))) *
                               uiScaleStep,
                           minUiScale, maxUiScale);
        }
    }
    std::string runner;
    read.text(wineRunnerKey, runner, "using the newest GE-Proton");
    settings.wineRunner = runner;
    read.pathsByName(wineRunnerPinsKey, settings.wineRunnerPins, "no game is pinned to a runner");
    read.boolean(keepGeProtonKey, settings.keepGeProtonCurrent, "keeping GE-Proton up to date");
    std::vector<std::string> ownPrefixes;
    read.texts(ownWinePrefixKey, ownPrefixes, "every game shares the Wine prefix");
    settings.ownWinePrefixes = library::wine::OwnPrefixes{{ownPrefixes.begin(), ownPrefixes.end()}};
    read.shortcuts(shortcutsKey, settings.shortcuts);
    read.plugins(pluginsKey, settings.plugins);
    read.integrations(integrationsKey, settings.integrations);
    read.boolean(installedOnlyKey, settings.view.installedOnly, "showing every game");
    read.text(sourceKey, settings.view.source, "showing every source");
    std::vector<std::string> hidden;
    read.texts(hiddenKey, hidden, "no game is hidden");
    settings.hidden = library::HiddenGames{std::move(hidden)};
    library::PlayHistory::Entries played;
    read.times(lastPlayedKey, played, "no launch is remembered");
    settings.lastPlayed = library::PlayHistory{std::move(played)};

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
    std::string keyboardSpelling;
    read.text(keyboardStyleKey, keyboardSpelling, "using the daisy wheel");
    if (!keyboardSpelling.empty()) {
        if (const std::optional<input::KeyboardStyle> style =
                input::keyboardStyleOf(keyboardSpelling)) {
            settings.keyboardStyle = *style;
        } else {
            lucent::warn("settings", "{}: {} is not a keyboard; using the daisy wheel",
                         file_.string(), keyboardSpelling);
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
            folders[library::key(store)] = own.string();
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
    json pins = json::object();
    for (const auto& [game, runner] : settings.wineRunnerPins) {
        pins[game] = runner.string();
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
        {uiSoundsKey, settings.uiSounds},
        {romFoldersKey, listOf(settings.romFolders)},
        {steamRootsKey, listOf(settings.steamRoots)},
        {installFoldersKey, folders},
        {uiScaleKey, settings.uiScale},
        {uiScaleBaseKey, uiScaleBaseVersion},
        {keyboardStyleKey, std::string{input::key(settings.keyboardStyle)}},
        {wineRunnerKey, settings.wineRunner.string()},
        {ownWinePrefixKey, settings.ownWinePrefixes.ids()},
        {wineRunnerPinsKey, pins},
        {keepGeProtonKey, settings.keepGeProtonCurrent},
        {shortcutsKey, {{shortcutKeysKey, shortcutKeys}, {shortcutPadsKey, shortcutPads}}}};
    json plugins = json::object();
    for (const auto& [id, choice] : settings.plugins) {
        plugins[id] = {{enabledKey, choice.enabled}, {grantedKey, choice.granted}};
    }
    document[pluginsKey] = plugins;
    if (const std::optional<IntegrationChoices>& chosen = settings.integrations) {
        json integrations{{steamKey, chosen->steam},
                          {epicKey, chosen->epic},
                          {gogKey, chosen->gog},
                          {standaloneEmulatorsKey, chosen->emulators},
                          {luaPluginsKey, chosen->plugins}};
        if (chosen->steamMode) {
            integrations[steamModeKey] = std::string{key(*chosen->steamMode)};
        }
        if (chosen->steamPads) {
            integrations[steamPadsKey] = std::string{key(*chosen->steamPads)};
        }
        document[integrationsKey] = integrations;
    }
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

float uiScaleFactor(int percent) noexcept {
    return static_cast<float>(percent) / 100.0f * uiScaleBase;
}

int steppedUiScale(int current, int step) noexcept {
    const int choices = (maxUiScale - minUiScale) / uiScaleStep + 1;
    const int from = std::clamp((current - minUiScale) / uiScaleStep, 0, choices - 1);
    const int to = (from + (step == 0 ? 1 : step) + choices) % choices;
    return minUiScale + to * uiScaleStep;
}

} // namespace opensu::settings

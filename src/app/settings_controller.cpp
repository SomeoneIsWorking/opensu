#include "settings_controller.hpp"

#include <algorithm>

#include "library/steam.hpp"
#include "settings/install_folders.hpp"
#include "ui/icon_size.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using ui::RowKind;
using ui::SettingsRow;

constexpr const char* layoutId = "layout";
constexpr const char* iconSizeId = "iconSize";
constexpr const char* scrollModeId = "scrollMode";
constexpr const char* pinDockId = "pinDock";
constexpr const char* uiScaleId = "uiScale";
constexpr const char* volumeId = "volume";
constexpr const char* muteId = "volumeMute";
constexpr const char* shortcutId = "shortcut.";
constexpr const char* shortcutsResetId = "shortcuts.reset";
constexpr const char* sortId = "sort";
constexpr const char* sourceId = "source";
constexpr const char* installedId = "installedOnly";
constexpr const char* hiddenId = "hidden";
constexpr const char* emulatorsId = "emulators";
constexpr const char* romAddId = "roms.add";
constexpr const char* romResetId = "roms.reset";
constexpr const char* steamAddId = "steam.add";
constexpr const char* steamResetId = "steam.reset";
constexpr const char* soundsId = "uiSounds";
constexpr const char* installDefaultId = "install.default";
constexpr const char* installStoreId = "install.";

SettingsRow toggle(const char* id, std::string label, std::string note, bool on) {
    SettingsRow row;
    row.id = id;
    row.kind = RowKind::Toggle;
    row.label = std::move(label);
    row.note = std::move(note);
    row.on = on;
    return row;
}

SettingsRow row(const char* id, RowKind kind, std::string label, std::string note,
                std::string value) {
    SettingsRow made;
    made.id = id;
    made.kind = kind;
    made.label = std::move(label);
    made.note = std::move(note);
    made.value = std::move(value);
    return made;
}

/// The paths of `folders` for a value, or `none` when there are no paths.
std::string listed(const std::vector<fs::path>& folders, const std::string& none) {
    std::string text;
    for (const fs::path& folder : folders) {
        text += (text.empty() ? "" : ", ") + folder.string();
    }
    return text.empty() ? none : text;
}

std::string storeKeyOf(library::Source store) {
    return std::string{installStoreId} + std::string{library::label(store)};
}

std::string percentOf(int percent) {
    return std::to_string(percent) + "%";
}

/// What a shortcut row reads: its key, and its pad chord when it has one.
std::string shortcutValue(const input::Shortcuts& shortcuts, input::Action action) {
    const std::optional<input::Combo> primary = shortcuts.primary(action);
    std::string value = primary ? input::describe(*primary) : "";
    if (const std::optional<input::PadChord> chord = shortcuts.padChord(action)) {
        value += (value.empty() ? "" : "  ") + input::describe(*chord);
    }
    return value;
}

std::string countOf(std::size_t count, const char* what) {
    return std::to_string(count) + " " + what + (count == 1 ? "" : "s");
}

/// Steps `value` by `step` (A is 1) around `count` choices.
std::size_t around(std::size_t value, int step, std::size_t count) {
    const auto size = static_cast<int>(count);
    return static_cast<std::size_t>((static_cast<int>(value) + (step == 0 ? 1 : step) + size) %
                                    size);
}

} // namespace

std::string SettingsController::category() const {
    return page_.isOpen() ? page_.focusedCategory().label : std::string{};
}

bool SettingsController::changes() const {
    const ui::SettingsRow* focused = page_.focusedRow();
    return focused != nullptr && focused->kind != RowKind::Info;
}

std::vector<ui::SettingsCategory> SettingsController::build() const {
    return {appearancePage(), libraryPage(), audioPage(),
            controlsPage(),   installPage(), aboutPage()};
}

ui::SettingsCategory SettingsController::appearancePage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"appearance", "Appearance", {}};
    page.rows.push_back(row(layoutId, RowKind::Choice, "Library layout",
                            "How Library lays out its tiles",
                            std::string{library::label(v.libraryMode)}));
    SettingsRow size =
        row(iconSizeId, RowKind::Slider, "Icon size", "How large Library's tiles are", "");
    size.level = v.iconSize;
    size.low = ui::minIconLevel;
    size.high = ui::maxIconLevel;
    page.rows.push_back(size);
    const config::HomeMode mode = v.homeMode.value_or(defaults_.homeMode);
    page.rows.push_back(row(scrollModeId, RowKind::Choice, "Scroll mode",
                            v.homeMode ? "How the home grid scrolls" : "From OPENSU_HOME_MODE",
                            mode == config::HomeMode::WiiSu ? "Pages" : "Scrolling"));
    page.rows.push_back(toggle(pinDockId, "Pin navigation bar on Library",
                               "Keep the dock up on Library", v.pinLibraryDock));
    page.rows.push_back(row(uiScaleId, RowKind::Choice, "Interface size",
                            "Scales everything on screen", percentOf(v.uiScale)));
    return page;
}

ui::SettingsCategory SettingsController::libraryPage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"library", "Library", {}};
    page.rows.push_back(row(sortId, RowKind::Choice, "Sort by", "How games are ordered",
                            std::string{library::label(v.view.sort)}));
    std::string source = "All sources";
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == v.view.source) {
            source = choice.label;
        }
    }
    page.rows.push_back(row(sourceId, RowKind::Choice, "Show", "Which store's games show", source));
    page.rows.push_back(toggle(installedId, "Installed games only",
                               "Hide games you have not installed", v.view.installedOnly));
    page.rows.push_back(
        row(hiddenId, RowKind::Action, "Show hidden games again", "Brings back every game you hid",
            v.hidden.keys().empty() ? "None hidden" : countOf(v.hidden.keys().size(), "game")));
    page.rows.push_back(row(emulatorsId, RowKind::Action, "Reset emulator choices",
                            "Every ROM goes back to its default emulator",
                            v.emulators.entries().empty()
                                ? "None chosen"
                                : countOf(v.emulators.entries().size(), "game")));
    page.rows.push_back(row(
        "roms.list", RowKind::Info, "ROM folders",
        v.romFolders.empty() && !defaults_.romRoots.empty() ? "From OPENSU_ROM_ROOTS"
                                                            : "Applies after a restart",
        listed(v.romFolders.empty() ? defaults_.romRoots : v.romFolders, "Found automatically")));
    page.rows.push_back(row(romAddId, RowKind::Folder, "Add a ROM folder",
                            "A folder with one subfolder per system", ""));
    page.rows.push_back(row(romResetId, RowKind::Action, "Find ROM folders automatically",
                            "Forgets the folders you added", ""));
    page.rows.push_back(row(
        "steam.list", RowKind::Info, "Steam install folders",
        v.steamRoots.empty() && !defaults_.steamRoots.empty() ? "From OPENSU_STEAM_ROOTS"
                                                              : "Applies after a restart",
        listed(v.steamRoots.empty() ? defaults_.steamRoots : v.steamRoots, "Found automatically")));
    page.rows.push_back(row(steamAddId, RowKind::Folder, "Add a Steam install folder",
                            "The folder that holds steamapps", ""));
    page.rows.push_back(row(steamResetId, RowKind::Action, "Find Steam automatically",
                            "Forgets the folders you added", ""));
    for (const fs::path& library : libraries_) {
        page.rows.push_back(row("steam.library", RowKind::Info, "Steam library",
                                "Manage them in Steam's storage settings", library.string()));
    }
    return page;
}

ui::SettingsCategory SettingsController::audioPage() const {
    ui::SettingsCategory page{"audio", "Audio", {}};
    page.rows.push_back(toggle(soundsId, "UI sounds", "The sounds menus and launches make",
                               preferences_.values().uiSounds));
    const audio::SystemVolume& volume = volume_.system();
    if (!volume.available()) {
        page.rows.push_back(row("volume.missing", RowKind::Info, "System volume",
                                volume.unavailable(), "Not available"));
        return page;
    }
    const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
    SettingsRow level =
        row(volumeId, RowKind::Slider, "System volume", "The output volume of this computer", "");
    level.level = state.percent;
    level.low = 0;
    level.high = 100;
    page.rows.push_back(level);
    page.rows.push_back(toggle(muteId, "Mute", "Silences the output", state.muted));
    return page;
}

ui::SettingsCategory SettingsController::controlsPage() const {
    ui::SettingsCategory page{"controls", "Controls", {}};
    for (const input::Action action : input::allActions) {
        const bool listening = shortcutEditor_.target() == action;
        std::string value = shortcutValue(shortcuts_, action);
        if (listening) {
            value = input::takesPadChord(action) ? "Press a key, or L2 + a button"
                                                 : "Press the new key";
        }
        page.rows.push_back(
            row((std::string{shortcutId} + std::string{input::spelling(action)}).c_str(),
                RowKind::Action, std::string{input::label(action)},
                listening ? "Esc or B cancels" : "", value));
    }
    page.rows.push_back(row(shortcutsResetId, RowKind::Action, "Restore default shortcuts",
                            "Every key and chord back to how it shipped", ""));
    return page;
}

ui::SettingsCategory SettingsController::installPage() const {
    const settings::InstallFolders& folders = preferences_.values().installFolders;
    ui::SettingsCategory page{"installs", "Install folders", {}};
    page.rows.push_back(row(installDefaultId, RowKind::Folder, "Default install folder",
                            "Where games install unless a store has its own folder",
                            folders.defaultFolder().empty() ? "Each store's own"
                                                            : folders.defaultFolder().string()));
    const char* notes[] = {"Added to Steam as a library", "Games go under this folder",
                           "Each game gets a folder of its id"};
    std::size_t at = 0;
    for (const library::Source store : settings::installStores) {
        const fs::path own = folders.storeFolder(store);
        std::string value = own.string();
        if (own.empty()) {
            value = folders.defaultFolder().empty()
                        ? "Store's own"
                        : "Default (" + folders.defaultFolder().string() + ")";
        }
        page.rows.push_back(row(storeKeyOf(store).c_str(), RowKind::Folder,
                                std::string{library::label(store)} + " install folder", notes[at++],
                                value));
    }
    return page;
}

ui::SettingsCategory SettingsController::aboutPage() const {
    return {"about",
            "About",
            {row("about.settings", RowKind::Info, "Settings file", "",
                 (defaults_.configDir / "settings.json").string()),
             row("about.data", RowKind::Info, "Data folder", "Sign-ins and GOG installs",
                 defaults_.dataDir.string()),
             row("about.cache", RowKind::Info, "Cache folder", "Artwork and sounds",
                 defaults_.cacheDir.string())}};
}

void SettingsController::open() {
    libraries_ = hooks_.steamLibraries();
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(build());
}

void SettingsController::close() {
    if (!page_.isOpen()) {
        return;
    }
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    page_.close();
}

void SettingsController::refresh() {
    if (page_.isOpen()) {
        page_.refresh(build());
    }
}

void SettingsController::changed() {
    preferences_.save();
    hooks_.applyLayout();
    refresh();
}

void SettingsController::showCategories() {
    if (page_.leaveRows()) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void SettingsController::act(gamepad::Button button) {
    if (shortcutEditor_.capturing()) {
        return;
    }
    const bool inRows = page_.zone() == ui::SettingsZone::Rows;
    const ui::SettingsRow* focused = page_.focusedRow();
    switch (button) {
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        // input-sound.md 3.4 Navigation: the first press of a D-pad key in Settings.
        if (page_.move(button == gamepad::Button::Down ? 1 : -1)) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::Left:
    case gamepad::Button::Right: {
        const int step = button == gamepad::Button::Left ? -1 : 1;
        if (!inRows) {
            if (step > 0 && page_.enterRows()) {
                sounds_.play(audio::Effect::Navigation);
            }
        } else if (focused != nullptr &&
                   (focused->kind == RowKind::Choice || focused->kind == RowKind::Slider)) {
            run(*focused, step);
        } else if (step < 0) {
            showCategories();
        }
        break;
    }
    case gamepad::Button::A:
        if (!inRows) {
            if (page_.enterRows()) {
                sounds_.play(audio::Effect::Navigation);
            }
        } else if (focused != nullptr) {
            run(*focused, 0);
        }
        break;
    case gamepad::Button::B:
        if (inRows) {
            showCategories();
        } else {
            close();
        }
        break;
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

void SettingsController::chooseLevel(int level) {
    const ui::SettingsRow* focused = page_.focusedRow();
    if (focused == nullptr || focused->kind != RowKind::Slider) {
        return;
    }
    const int next = std::clamp(level, focused->low, focused->high);
    if (focused->id == volumeId) {
        if (const std::string refused = volume_.system().setPercent(next); !refused.empty()) {
            hooks_.say(refused, true);
        }
        return;
    }
    if (next != preferences_.values().iconSize) {
        sounds_.play(audio::Effect::Navigation);
        preferences_.values().iconSize = next;
        changed();
    }
}

void SettingsController::stepIconSize(int delta) {
    chooseLevel(preferences_.values().iconSize + delta);
}

void SettingsController::run(const ui::SettingsRow& focused, int step) {
    // The row is a reference into the page, which a change rebuilds.
    const std::string id = focused.id;
    if (id == iconSizeId) {
        if (step != 0) {
            stepIconSize(step);
        }
        return;
    }
    if (id == volumeId || id == muteId) {
        runAudio(id, step);
        return;
    }
    if (id.starts_with(shortcutId) || id == shortcutsResetId) {
        runControls(id);
        return;
    }
    if (focused.kind == RowKind::Folder) {
        if (step == 0) {
            chooseFolder(id);
        }
        return;
    }
    if (focused.kind == RowKind::Info) {
        return;
    }
    if (id == layoutId || id == scrollModeId || id == pinDockId || id == soundsId ||
        id == uiScaleId) {
        runAppearance(id, step);
    } else {
        runLibrary(id, step);
    }
}

void SettingsController::runAppearance(const std::string& id, int step) {
    settings::Settings& v = preferences_.values();
    const bool toggles = id == pinDockId || id == soundsId;
    if (toggles && step != 0) {
        return;
    }
    sounds_.play(audio::Effect::Navigation);
    if (id == layoutId) {
        v.libraryMode = library::allLibraryModes[around(static_cast<std::size_t>(v.libraryMode),
                                                        step, library::allLibraryModes.size())];
        hooks_.reshow();
    } else if (id == scrollModeId) {
        v.homeMode = v.homeMode.value_or(defaults_.homeMode) == config::HomeMode::WiiSu
                         ? config::HomeMode::Standard
                         : config::HomeMode::WiiSu;
    } else if (id == pinDockId) {
        v.pinLibraryDock = !v.pinLibraryDock;
    } else if (id == uiScaleId) {
        const int levels =
            (settings::maxUiScale - settings::minUiScale) / settings::uiScaleStep + 1;
        const auto choices = static_cast<std::size_t>(levels);
        const auto from =
            static_cast<std::size_t>((v.uiScale - settings::minUiScale) / settings::uiScaleStep);
        v.uiScale = settings::minUiScale +
                    static_cast<int>(around(from, step, choices)) * settings::uiScaleStep;
    } else {
        v.uiSounds = !v.uiSounds;
    }
    changed();
}

void SettingsController::runAudio(const std::string& id, int step) {
    std::string refused;
    if (id == volumeId) {
        if (step != 0) {
            refused = volume_.system().step(step * audio::volumeStep);
        }
    } else if (step == 0) {
        refused = volume_.system().toggleMute();
    }
    if (!refused.empty()) {
        hooks_.say(refused, true);
    }
    refresh();
}

void SettingsController::runControls(const std::string& id) {
    if (id == shortcutsResetId) {
        shortcutEditor_.resetAll();
    } else if (const std::optional<input::Action> action = input::actionOf(
                   std::string_view{id}.substr(std::string_view{shortcutId}.size()))) {
        sounds_.play(audio::Effect::Navigation);
        shortcutEditor_.begin(*action);
    }
    refresh();
}

void SettingsController::runLibrary(const std::string& id, int step) {
    settings::Settings& v = preferences_.values();
    if ((id == installedId || id == hiddenId || id == emulatorsId || id == romResetId ||
         id == steamResetId) &&
        step != 0) {
        return;
    }
    sounds_.play(audio::Effect::Navigation);
    if (id == sortId) {
        v.view.sort = library::allSortKeys[around(static_cast<std::size_t>(v.view.sort), step,
                                                  library::allSortKeys.size())];
        hooks_.reshow();
    } else if (id == sourceId) {
        const std::vector<library::SourceChoice> choices = hooks_.sourceChoices();
        const auto at = std::ranges::find(choices, v.view.source, &library::SourceChoice::key);
        const std::size_t from = at == choices.end()
                                     ? choices.size() - 1
                                     : static_cast<std::size_t>(at - choices.begin());
        v.view.source = choices[around(from, step, choices.size())].key;
        hooks_.reshow();
    } else if (id == installedId) {
        v.view.installedOnly = !v.view.installedOnly;
        hooks_.reshow();
    } else if (id == hiddenId) {
        v.hidden = library::HiddenGames{};
        hooks_.reshow();
    } else if (id == emulatorsId) {
        v.emulators = library::EmulatorChoices{};
        hooks_.reloadCatalog();
    } else if (id == romResetId) {
        v.romFolders.clear();
        hooks_.say("ROM folders are found automatically after a restart", false);
    } else if (id == steamResetId) {
        v.steamRoots.clear();
        hooks_.say("Steam is found automatically after a restart", false);
    }
    changed();
}

void SettingsController::chooseFolder(const std::string& id) {
    if (id == romAddId || id == steamAddId) {
        addRoot(id);
    } else if (id == installDefaultId) {
        editInstallFolder("Default install folder", id);
    } else {
        editInstallFolder(page_.focusedRow()->label, id);
    }
}

void SettingsController::addRoot(const std::string& id) {
    const bool steam = id == steamAddId;
    PathEditor::Request request;
    request.title = steam ? "Add a Steam install folder" : "Add a ROM folder";
    request.start = defaults_.home;
    request.refusal = [steam, this](const fs::path& folder) {
        std::string refused = settings::refusal(folder, settings::Need::Read);
        if (refused.empty() && steam &&
            library::steam::Library::discover(defaults_.home, {folder}).roots().empty()) {
            refused = "that folder holds no Steam installation";
        }
        return refused;
    };
    request.done = [steam, this](std::optional<fs::path> folder) {
        if (!folder) {
            return;
        }
        std::vector<fs::path>& roots =
            steam ? preferences_.values().steamRoots : preferences_.values().romFolders;
        if (std::ranges::find(roots, *folder) == roots.end()) {
            roots.push_back(*folder);
        }
        hooks_.say((steam ? "Steam folder" : "ROM folder") +
                       std::string{" saved; it applies after a restart"},
                   false);
        changed();
    };
    paths_.begin(std::move(request));
}

void SettingsController::editInstallFolder(const std::string& title, const std::string& id) {
    settings::InstallFolders& folders = preferences_.values().installFolders;
    const bool general = id == installDefaultId;
    std::optional<library::Source> store;
    for (const library::Source candidate : settings::installStores) {
        if (id == storeKeyOf(candidate)) {
            store = candidate;
        }
    }
    const fs::path now = store ? folders.storeFolder(*store) : folders.defaultFolder();
    PathEditor::Request request;
    request.title = title;
    request.start = !now.empty() ? now : defaults_.home;
    request.clearLabel = general ? "Let each store choose" : "Follow the default";
    request.refusal = [](const fs::path& folder) {
        return settings::refusal(folder);
    };
    request.done = [store, this](const std::optional<fs::path>& folder) {
        settings::InstallFolders& chosen = preferences_.values().installFolders;
        if (store) {
            chosen.setStore(*store, folder.value_or(fs::path{}));
        } else {
            chosen.setDefault(folder.value_or(fs::path{}));
        }
        changed();
    };
    paths_.begin(std::move(request));
}

} // namespace opensu::app

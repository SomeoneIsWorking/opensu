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
constexpr const char* chordsOpenId = "chords.open";
constexpr const char* keyboardId = "keyboard";
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
    if (!page_.isOpen()) {
        return {};
    }
    const ui::SettingsCategory& focused = page_.focusedCategory();
    return layers_.isOpen() && focused.id == EmulatorSettings::categoryId
               ? focused.label + " › " + layers_.openName()
               : focused.label;
}

bool SettingsController::changes() const {
    const ui::SettingsRow* focused = page_.focusedRow();
    return focused != nullptr && focused->kind != RowKind::Info;
}

std::vector<ui::SettingsCategory> SettingsController::build() const {
    const settings::Integrations& in = inForce_.current();
    ui::SettingsCategory emulators = emulators_.page(layers_.openRows());
    if (std::optional<ui::SettingsCategory> layer = layers_.page(emulators.id, emulators.label)) {
        emulators = std::move(*layer);
    }
    std::vector<ui::SettingsCategory> pages{appearancePage(), libraryPage(), integrations_.page(),
                                            std::move(emulators)};
    if (in.lists(library::Source::Steam)) {
        pages.push_back(steamLibraries_.page());
    }
    pages.push_back(audioPage());
    pages.push_back(controlsPage());
    pages.push_back(chords_.page());
    pages.push_back(installPage());
    pages.push_back(artwork_.page());
    // Wine runs the Windows builds of GOG and Epic games.
    if (in.epic || in.gog) {
        pages.push_back(wine_.page());
    }
    if (in.plugins) {
        pages.push_back(plugins_.page());
    }
    pages.push_back(aboutPage());
    return pages;
}

ui::SettingsCategory SettingsController::appearancePage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"appearance", "Appearance", {}};
    page.rows.push_back(ui::makeRow(layoutId, RowKind::Choice, "Library layout",
                                    "How Library lays out its tiles",
                                    std::string{library::label(v.libraryMode)}));
    page.rows.push_back(
        ui::makeSlider(iconSizeId, "Icon size", "How large Library's tiles are",
                       ui::SliderRange{v.iconSize, ui::minIconLevel, ui::maxIconLevel}));
    const config::HomeMode mode = v.homeMode.value_or(defaults_.homeMode);
    page.rows.push_back(
        ui::makeRow(scrollModeId, RowKind::Choice, "Scroll mode",
                    v.homeMode ? "How the home grid scrolls" : "From OPENSU_HOME_MODE",
                    mode == config::HomeMode::WiiSu ? "Pages" : "Scrolling"));
    page.rows.push_back(ui::makeToggle(pinDockId, "Pin navigation bar on Library",
                                       "Keep the dock up on Library", v.pinLibraryDock));
    page.rows.push_back(ui::makeRow(uiScaleId, RowKind::Choice, "Interface size",
                                    "Scales everything on screen", percentOf(v.uiScale)));
    return page;
}

ui::SettingsCategory SettingsController::libraryPage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"library", "Library", {}};
    page.rows.push_back(ui::makeRow(sortId, RowKind::Choice, "Sort by", "How games are ordered",
                                    std::string{library::label(v.view.sort)}));
    std::string source = "All sources";
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == v.view.source) {
            source = choice.label;
        }
    }
    page.rows.push_back(
        ui::makeRow(sourceId, RowKind::Choice, "Show", "Which store's games show", source));
    page.rows.push_back(ui::makeToggle(installedId, "Installed games only",
                                       "Hide games you have not installed", v.view.installedOnly));
    page.rows.push_back(ui::makeRow(
        hiddenId, RowKind::Action, "Show hidden games again", "Brings back every game you hid",
        v.hidden.keys().empty() ? "None hidden" : countOf(v.hidden.keys().size(), "game")));
    page.rows.push_back(ui::makeRow(
        emulatorsId, RowKind::Action, "Reset emulator choices",
        "Every ROM goes back to its default emulator",
        layers_.gamePicks() == 0 ? "None chosen" : countOf(layers_.gamePicks(), "game")));
    page.rows.push_back(ui::makeRow(
        "roms.list", RowKind::Info, "ROM folders",
        v.romFolders.empty() && !defaults_.romRoots.empty() ? "From OPENSU_ROM_ROOTS"
                                                            : "Applies after a restart",
        listed(v.romFolders.empty() ? defaults_.romRoots : v.romFolders, "Found automatically")));
    page.rows.push_back(ui::makeRow(romAddId, RowKind::Folder, "Add a ROM folder",
                                    "A folder with one subfolder per system", ""));
    page.rows.push_back(ui::makeRow(romResetId, RowKind::Action, "Find ROM folders automatically",
                                    "Forgets the folders you added", ""));
    if (!inForce_.current().lists(library::Source::Steam)) {
        return page;
    }
    page.rows.push_back(ui::makeRow(
        "steam.list", RowKind::Info, "Steam install folders",
        v.steamRoots.empty() && !defaults_.steamRoots.empty() ? "From OPENSU_STEAM_ROOTS"
                                                              : "Applies after a restart",
        listed(v.steamRoots.empty() ? defaults_.steamRoots : v.steamRoots, "Found automatically")));
    page.rows.push_back(ui::makeRow(steamAddId, RowKind::Folder, "Add a Steam install folder",
                                    "The folder that holds steamapps", ""));
    page.rows.push_back(ui::makeRow(steamResetId, RowKind::Action, "Find Steam automatically",
                                    "Forgets the folders you added", ""));
    return page;
}

ui::SettingsCategory SettingsController::audioPage() const {
    ui::SettingsCategory page{"audio", "Audio", {}};
    page.rows.push_back(ui::makeToggle(soundsId, "UI sounds", "The sounds menus and launches make",
                                       preferences_.values().uiSounds));
    const audio::SystemVolume& volume = volume_.system();
    if (!volume.available()) {
        page.rows.push_back(ui::makeRow("volume.missing", RowKind::Info, "System volume",
                                        volume.unavailable(), "Not available"));
        return page;
    }
    const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
    page.rows.push_back(ui::makeSlider(volumeId, "System volume",
                                       "The output volume of this computer",
                                       ui::SliderRange{state.percent, 0, 100}));
    page.rows.push_back(ui::makeToggle(muteId, "Mute", "Silences the output", state.muted));
    return page;
}

ui::SettingsCategory SettingsController::controlsPage() const {
    ui::SettingsCategory page{"controls", "Controls", mappings_.settingsRows()};
    page.rows.push_back(ui::makeRow(
        keyboardId, RowKind::Choice, "Keyboard", "The on-screen keyboard the fields open",
        std::string{input::label(preferences_.values().keyboardStyle)}));
    for (const input::Action action : input::allActions) {
        const bool listening = shortcutEditor_.target() == action;
        std::string value = shortcutValue(shortcuts_, action);
        std::string note;
        if (listening && input::takesPadChord(action)) {
            value = "Press a key, or Guide + a button";
            note = ShortcutEditor::listeningNote;
        } else if (listening) {
            value = "Press the new key";
            note = "Esc or B cancels";
        }
        page.rows.push_back(
            ui::makeRow((std::string{shortcutId} + std::string{input::spelling(action)}).c_str(),
                        RowKind::Action, std::string{input::label(action)}, note, value));
    }
    page.rows.push_back(ui::makeRow(chordsOpenId, RowKind::Action, "Guide chords",
                                    "What Guide and a button do, over a game too", "Open"));
    page.rows.push_back(ui::makeRow(shortcutsResetId, RowKind::Action, "Restore default shortcuts",
                                    "Every key and chord back to how it shipped", ""));
    return page;
}

ui::SettingsCategory SettingsController::installPage() const {
    const settings::InstallFolders& folders = preferences_.values().installFolders;
    const settings::StoreFolders own = hooks_.storeFolders();
    ui::SettingsCategory page{"installs", "Install folders", {}};
    page.rows.push_back(ui::makeRow(installDefaultId, RowKind::Folder, "Default install folder",
                                    "Where games install unless a store has its own folder",
                                    folders.defaultFolder().empty()
                                        ? "Not set: each store's own"
                                        : folders.defaultFolder().string()));
    for (const library::Source store : settings::installStores) {
        if (!inForce_.current().lists(store)) {
            continue;
        }
        const settings::Destination destination = folders.resolve(store, own);
        std::string note;
        switch (destination.origin) {
        case settings::FolderOrigin::Store:
            note = "Set for this store";
            break;
        case settings::FolderOrigin::Default:
            note = "From the default install folder";
            break;
        case settings::FolderOrigin::StoreOwn:
            note = std::string{library::label(store)} + "'s own default";
            break;
        }
        if (store == library::Source::Steam) {
            note += "; added to Steam as a library";
        }
        std::string value = destination.folder.string();
        if (value.empty()) {
            value = "Steam's default library (shown while Steam runs)";
        }
        page.rows.push_back(ui::makeRow(storeKeyOf(store), RowKind::Folder,
                                        std::string{library::label(store)} + " install folder",
                                        note, value));
    }
    return page;
}

ui::SettingsCategory SettingsController::aboutPage() const {
    return {"about",
            "About",
            {ui::makeRow("about.settings", RowKind::Info, "Settings file", "",
                         (defaults_.configDir / "settings.json").string()),
             ui::makeRow("about.data", RowKind::Info, "Data folder", "Sign-ins and GOG installs",
                         defaults_.dataDir.string()),
             ui::makeRow("about.cache", RowKind::Info, "Cache folder", "Artwork and sounds",
                         defaults_.cacheDir.string())}};
}

void SettingsController::open() {
    emulators_.rescan();
    layers_.close();
    if (inForce_.current().lists(library::Source::Steam)) {
        steamLibraries_.reload();
    }
    if (inForce_.current().epic || inForce_.current().gog) {
        wine_.rescan();
    }
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    page_.open(build());
}

void SettingsController::openCategory(const std::string& id) {
    open();
    if (const std::optional<std::size_t> index = categoryIndex(id)) {
        page_.focusCategory(*index);
    }
}

bool SettingsController::showCategory(std::string_view id) {
    const std::optional<std::size_t> index = categoryIndex(id);
    if (!index) {
        return false;
    }
    page_.focusCategory(*index);
    return page_.enterRows();
}

std::optional<std::size_t> SettingsController::categoryIndex(std::string_view id) const {
    const std::vector<ui::SettingsCategory>& categories = page_.categories();
    for (std::size_t index = 0; index < categories.size(); ++index) {
        if (categories[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
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

void SettingsController::service() {
    steamLibraries_.service();
    artwork_.service();
}

void SettingsController::changed() {
    preferences_.save();
    hooks_.applyLayout();
    refresh();
}

void SettingsController::focusLayerRow() {
    const std::string target = layers_.returnRow();
    const std::vector<ui::SettingsRow>& rows = page_.focusedCategory().rows;
    const auto at = std::ranges::find(rows, target, &ui::SettingsRow::id);
    page_.focusRow(at == rows.end() ? 0 : static_cast<std::size_t>(at - rows.begin()));
}

void SettingsController::closeLayer() {
    const std::string opener = layers_.openRowId();
    layers_.close();
    sounds_.play(audio::Effect::Navigation);
    refresh();
    const std::vector<ui::SettingsRow>& rows = page_.focusedCategory().rows;
    const auto at = std::ranges::find(rows, opener, &ui::SettingsRow::id);
    page_.focusRow(at == rows.end() ? 0 : static_cast<std::size_t>(at - rows.begin()));
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
            emulators_.disarm();
            steamLibraries_.disarm();
            refresh();
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
        if (inRows && layers_.isOpen() &&
            page_.focusedCategory().id == EmulatorSettings::categoryId) {
            switch (layers_.back()) {
            case LayerSettings::Back::Within:
                sounds_.play(audio::Effect::Navigation);
                refresh();
                focusLayerRow();
                break;
            case LayerSettings::Back::Layer:
                closeLayer();
                break;
            }
        } else if (inRows) {
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
    if (id == keyboardId) {
        settings::Settings& v = preferences_.values();
        sounds_.play(audio::Effect::Navigation);
        const auto from = static_cast<std::size_t>(v.keyboardStyle);
        v.keyboardStyle =
            input::allKeyboardStyles[around(from, step, input::allKeyboardStyles.size())];
        changed();
        return;
    }
    if (id.starts_with(shortcutId) || id == shortcutsResetId || id == chordsOpenId) {
        runControls(id);
        return;
    }
    if (MappingEditor::owns(id)) {
        sounds_.play(audio::Effect::Navigation);
        mappings_.runSetting(id, step);
        refresh();
        return;
    }
    if (ChordSettings::owns(id)) {
        if (step == 0) {
            sounds_.play(audio::Effect::Navigation);
            chords_.run(id, step);
            refresh();
        }
        return;
    }
    if (LayerSettings::owns(id)) {
        if (focused.kind == RowKind::Info) {
            return;
        }
        sounds_.play(audio::Effect::Navigation);
        const bool wasOpen = layers_.isOpen();
        const std::string before = layers_.openName();
        layers_.run(id, step);
        refresh();
        if (!wasOpen && layers_.isOpen()) {
            page_.focusRow(0);
        } else if (layers_.openName() != before) {
            focusLayerRow();
        }
        return;
    }
    if (IntegrationSettings::owns(id)) {
        if (focused.kind != RowKind::Info) {
            sounds_.play(audio::Effect::Navigation);
            integrations_.run(id, step);
        }
        return;
    }
    if (ArtworkSettings::owns(id)) {
        if (focused.kind != RowKind::Info && step == 0) {
            sounds_.play(audio::Effect::Navigation);
            artwork_.run(id, step);
        }
        return;
    }
    if (EmulatorSettings::owns(id) || SteamLibrarySettings::owns(id) || PluginSettings::owns(id) ||
        WineSettings::owns(id)) {
        if (focused.kind == RowKind::Info) {
            return;
        }
        sounds_.play(audio::Effect::Navigation);
        if (EmulatorSettings::owns(id)) {
            emulators_.run(id, step);
        } else if (SteamLibrarySettings::owns(id)) {
            steamLibraries_.run(id, step);
        } else if (WineSettings::owns(id)) {
            wine_.run(id, step);
        } else {
            plugins_.run(id, step);
        }
        refresh();
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
        v.uiScale = settings::steppedUiScale(v.uiScale, step);
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
    if (id == chordsOpenId) {
        if (showCategory(ChordSettings::categoryId)) {
            sounds_.play(audio::Effect::Navigation);
        }
        return;
    }
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
        layers_.clearGamePicks();
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

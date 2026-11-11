#include "settings_controller.hpp"

#include <algorithm>
#include <memory>

#include "build_version.hpp"
#include "layered_editor.hpp"
#include "library/add_game_offer.hpp"
#include "library/steam.hpp"
#include "lucent/text.h"
#include "night_light.hpp"
#include "settings/install_folders.hpp"
#include "settings_screen.hpp"
#include "ui/interface_input.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using gpu2d::ui::Screen;
using ui::RowKind;
using ui::SettingsRow;

constexpr const char* scrollModeId = "scrollMode";
constexpr const char* nightLightId = "nightLight";
constexpr const char* shortcutId = "shortcut.";
constexpr const char* shortcutsResetId = "shortcuts.reset";
constexpr const char* keyboardId = "keyboard";
constexpr const char* hiddenId = "hidden";
constexpr const char* emulatorsId = "emulators";
constexpr const char* steamAddId = "steam.add";
constexpr const char* gameAddId = "games.add";
constexpr const char* gameForgetId = "games.forget";
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

/// Steps `value` by `step` (A is 1) around `count` choices.
std::size_t around(std::size_t value, int step, std::size_t count) {
    const auto size = static_cast<int>(count);
    return static_cast<std::size_t>(
        (static_cast<int>(value) + (step == 0 ? 1 : step) % size + size) % size);
}

} // namespace

const std::vector<ui::SettingsCategory>& SettingsController::categories() {
    if (!categories_) {
        categories_ = build();
    }
    return *categories_;
}

const ui::SettingsCategory& SettingsController::page(const std::string& id) {
    for (const ui::SettingsCategory& category : categories()) {
        if (category.id == id) {
            return category;
        }
    }
    if (const auto kept = pages_.find(id); kept != pages_.end()) {
        return kept->second;
    }
    std::optional<ui::SettingsCategory> built = subPage(id);
    return pages_.emplace(id, built ? std::move(*built) : ui::SettingsCategory{id, "", {}})
        .first->second;
}

std::optional<ui::SettingsCategory> SettingsController::subPage(const std::string& id) const {
    if (std::optional<ui::SettingsCategory> found = emulators_.subPage(id)) {
        return found;
    }
    if (std::optional<ui::SettingsCategory> found = layers_.listPage(id)) {
        return found;
    }
    if (std::optional<ui::SettingsCategory> found = wine_.subPage(id)) {
        return found;
    }
    if (std::optional<ui::SettingsCategory> found = lanRoms_.subPage(id)) {
        return found;
    }
    if (std::optional<ui::SettingsCategory> found = lanProfiles_.subPage(id)) {
        return found;
    }
    return mappings_.settingsPage(id);
}

std::string SettingsController::pageTitle(const std::string& id) {
    return page(id).label;
}

std::string SettingsController::category() const {
    std::string trail;
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->overlay()) {
            continue;
        }
        if (const auto* root = dynamic_cast<const SettingsScreen*>(screen)) {
            trail = root->categoryLabel();
        } else if (!trail.empty()) {
            trail += " › " + screen->title();
        }
    }
    return trail;
}

bool SettingsController::isOpen() const {
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    return std::ranges::any_of(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const SettingsScreen*>(screen) != nullptr;
    });
}

std::vector<ui::SettingsCategory> SettingsController::build() const {
    const settings::Integrations& in = inForce_.current();
    std::vector<ui::SettingsCategory> pages{appearancePage(), libraryPage(), integrations_.page(),
                                            emulatorsPage()};
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
    if (in.lan) {
        pages.push_back(lan_.page());
    }
    pages.push_back(online_.page());
    pages.push_back(profiles_.page());
    pages.push_back(saves_.page());
    pages.push_back(aboutPage());
    return pages;
}

ui::SettingsCategory SettingsController::appearancePage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"appearance", "Appearance", {}};
    page.rows.push_back(interfaceThemes_.row());
    page.rows.push_back(shelf_.layout("How Library lays out its tiles"));
    page.rows.push_back(shelf_.iconSize("How large the shelf's tiles are"));
    const config::HomeMode mode = interfaceThemes_.homeMode(defaults_.homeMode);
    page.rows.push_back(ui::makeChoice(
        scrollModeId, "Scroll mode", "How the home grid scrolls",
        ui::Options{{"Scrolling", "Pages"}, mode == config::HomeMode::WiiSu ? 1U : 0U}));
    page.rows.push_back(shelf_.pin("Keep the dock up on Library"));
    if (nightLightSupported()) {
        page.rows.push_back(
            ui::makeChoice(nightLightId, "Night light",
                           inForce_.loginSession()
                               ? "Warms the screen's colours at night"
                               : "For session mode; here the desktop's own night light applies",
                           ui::optionsOf(settings::allNightLightChoices, v.nightLight,
                                         [](settings::NightLightChoice choice) {
                                             return std::string{settings::label(choice)};
                                         })));
    }
    page.rows.push_back(
        ui::makeRow("appearance.scale", RowKind::Info, "Interface size", "In the Guide menu", ""));
    return page;
}

ui::SettingsCategory SettingsController::libraryPage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"library", "Library", {}};
    page.rows.push_back(ui::makeHeading("What shows"));
    page.rows.push_back(ui::makeRow("library.home", RowKind::Info, "Home",
                                    "Your installed games and consoles, plus what you add; Add to "
                                    "Home and Remove from Home are in a game's "
                                    "or console's menu and on a game's details page",
                                    ""));
    page.rows.push_back(shelf_.sort("How games are ordered"));
    page.rows.push_back(shelf_.source("Which store's games show"));
    page.rows.push_back(shelf_.installedOnly("Hide games you have not installed"));
    ui::SettingsRow hidden = ui::makeRow(
        hiddenId, RowKind::Action, "Show hidden games again", "Brings back every game you hid",
        v.hidden.keys().empty() ? "None hidden"
                                : lucent::text::counted(v.hidden.keys().size(), "game"));
    page.rows.push_back(std::move(hidden));
    ui::SettingsRow picks =
        ui::makeRow(emulatorsId, RowKind::Action, "Reset emulator choices",
                    "Every ROM goes back to its default emulator",
                    layers_.gamePicks() == 0 ? "None chosen"
                                             : lucent::text::counted(layers_.gamePicks(), "game"));
    if (layers_.gamePicks() > 0) {
        picks.confirm = "Every game's own emulator is forgotten; each console's stays.";
    }
    page.rows.push_back(std::move(picks));
    std::ranges::move(romFolders_.rows(), std::back_inserter(page.rows));
    if (!library::addGameOffer().empty()) {
        page.rows.push_back(ui::makeHeading("Game folders"));
        page.rows.push_back(ui::makeRow("games.list", RowKind::Info, "Game folders",
                                        "The library reads these each time it loads",
                                        listed(v.gameFolders, "None")));
        page.rows.push_back(ui::makeRow(gameAddId, RowKind::Folder, "Add a game folder",
                                        "A folder of game shortcuts, or of games", ""));
        if (!v.gameFolders.empty()) {
            page.rows.push_back(ui::makeRow(gameForgetId, RowKind::Action, "Forget game folders",
                                            "Their games leave the library; the files stay", ""));
        }
    }
    if (!inForce_.current().lists(library::Source::Steam)) {
        return page;
    }
    page.rows.push_back(ui::makeHeading("Steam"));
    page.rows.push_back(ui::makeRow(
        "steam.list", RowKind::Info, "Steam install folders",
        std::string{v.steamRoots.empty() && !defaults_.steamRoots.empty()
                        ? ui::startedWith
                        : std::string_view{"Applies after a restart"}},
        listed(v.steamRoots.empty() ? defaults_.steamRoots : v.steamRoots, "Found automatically")));
    page.rows.push_back(ui::makeRow(steamAddId, RowKind::Folder, "Add a Steam install folder",
                                    "The folder that holds steamapps", ""));
    page.rows.push_back(ui::makeRow(steamResetId, RowKind::Action, "Find Steam automatically",
                                    "Forgets the folders you added", ""));
    return page;
}

ui::SettingsCategory SettingsController::emulatorsPage() const {
    return emulators_.page(layers_.pageRows());
}

ui::SettingsCategory SettingsController::audioPage() const {
    ui::SettingsCategory page{"audio", "Audio", {}};
    page.rows.push_back(ui::makeToggle(soundsId, "UI sounds", "The sounds menus and launches make",
                                       preferences_.values().uiSounds));
    page.rows.push_back(interfaceSounds_.row());
    page.rows.push_back(interfaceSounds_.musicRow());
    page.rows.push_back(
        ui::makeRow("audio.volume", RowKind::Info, "Volume", "In the Guide menu", ""));
    return page;
}

ui::SettingsCategory SettingsController::controlsPage() const {
    ui::SettingsCategory page{"controls", "Controls", {}};
    page.rows.push_back(ui::makeHeading("Controllers"));
    std::ranges::move(mappings_.settingsRows(), std::back_inserter(page.rows));
    page.rows.push_back(
        ui::makeChoice(keyboardId, "Keyboard", "The on-screen keyboard the fields open",
                       ui::optionsOf(input::allKeyboardStyles, preferences_.values().keyboardStyle,
                                     [](input::KeyboardStyle style) {
                                         return std::string{input::label(style)};
                                     })));
    page.rows.push_back(ui::makeHeading("Shortcuts"));
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
    ui::SettingsRow reset =
        ui::makeRow(shortcutsResetId, RowKind::Action, "Restore default shortcuts",
                    "Every key and chord back to how it shipped", "");
    reset.confirm = "Every key and chord goes back to how it shipped.";
    page.rows.push_back(std::move(reset));
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
    return {
        "about",
        "About",
        {ui::makeRow("about.version", RowKind::Info, "Version", "This build of openSU",
                     buildVersion.empty() ? "Unknown" : std::string{buildVersion}),
         ui::makeHeading("Where openSU keeps things"),
         ui::makeRow("about.settings", RowKind::Info, "Settings file",
                     config::shownPath(defaults_.configDir / "settings.json", defaults_.home), ""),
         ui::makeRow("about.data", RowKind::Info, "Data folder: sign-ins and GOG installs",
                     config::shownPath(defaults_.dataDir, defaults_.home), ""),
         ui::makeRow("about.cache", RowKind::Info, "Cache folder: artwork and sounds",
                     config::shownPath(defaults_.cacheDir, defaults_.home), "")}};
}

void SettingsController::open() {
    show({});
}

void SettingsController::openCategory(const std::string& id) {
    show(id);
}

void SettingsController::openAt(const std::string& id, const std::string& row) {
    show(id, row);
}

void SettingsController::show(const std::string& category, const std::string& row) {
    emulators_.rescan();
    integrations_.sync();
    if (inForce_.current().lists(library::Source::Steam)) {
        steamLibraries_.reload();
    }
    if (inForce_.current().epic || inForce_.current().gog) {
        wine_.rescan();
    }
    refresh();
    if (isOpen()) {
        dropScreens();
    }
    const std::vector<ui::SettingsCategory>& all = categories();
    const bool known = std::ranges::any_of(all, [&category](const ui::SettingsCategory& page) {
        return page.id == category;
    });
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<SettingsScreen>(
        *this, chrome_, known ? category : all.front().id, known ? row : std::string{}));
}

void SettingsController::close() {
    if (!isOpen()) {
        return;
    }
    closeAll();
}

void SettingsController::closeAll() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    dropScreens();
}

void SettingsController::dropScreens() {
    if (!hooks_.overGuide()) {
        navigator_.clear();
        return;
    }
    const std::vector<const gpu2d::ui::Screen*> open = navigator_.screens();
    const auto found = std::ranges::find_if(open, [](const gpu2d::ui::Screen* screen) {
        return dynamic_cast<const SettingsScreen*>(screen) != nullptr;
    });
    if (found != open.end()) {
        navigator_.closeFrom(**found);
    }
}

Screen::BackReply SettingsController::leave() {
    const std::size_t waiting = integrations_.pendingCount();
    if (waiting == 0) {
        closeAll();
        return Screen::BackReply::Stay;
    }
    std::vector<gpu2d::ui::Dialog::Choice> choices{
        {"Apply",
         [this] {
             applyIntegrations();
             closeAll();
         },
         gpu2d::ui::ButtonKind::Primary},
        {"Discard",
         [this] {
             integrations_.sync();
             closeAll();
         },
         gpu2d::ui::ButtonKind::Danger},
        {"Keep editing", {}, gpu2d::ui::ButtonKind::Normal},
    };
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        "Apply integration changes?",
        lucent::text::counted(waiting, "change") + " on Integrations " +
            (waiting == 1 ? "is" : "are") + " not applied yet. Discarded changes are lost.",
        std::move(choices)));
    return Screen::BackReply::Stay;
}

void SettingsController::applyIntegrations() {
    const std::size_t applied = integrations_.apply();
    if (applied > 0) {
        hooks_.say("Integrations: " + lucent::text::counted(applied, "change") + " applied", false);
    }
    refresh();
}

void SettingsController::act(gamepad::Button button) {
    if (shortcutEditor_.capturing()) {
        return;
    }
    if (const std::optional<gpu2d::ui::Action> action =
            ui::interfaceActionOf(button, navigator_.typing())) {
        navigator_.act(*action);
    }
}

void SettingsController::refresh() {
    categories_.reset();
    pages_.clear();
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

void SettingsController::showCategories() {
    const auto* root = dynamic_cast<const SettingsScreen*>(navigator_.top());
    if (root == nullptr) {
        // Pages and what is over them close; the root shows its categories.
        while (navigator_.active() &&
               dynamic_cast<const SettingsScreen*>(navigator_.top()) == nullptr) {
            navigator_.pop();
        }
        root = dynamic_cast<const SettingsScreen*>(navigator_.top());
    }
    if (root != nullptr && navigator_.focus(root->tabId())) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void SettingsController::openLanImport() {
    show(std::string{LanSettings::categoryId}, {});
    if (std::optional<std::string> entry = lanRoms_.entry()) {
        openPage(*entry);
    }
}

void SettingsController::openPage(const std::string& id) {
    if (std::optional<EditorContext> context = layers_.contextOf(id)) {
        std::string title = layered_.scopeName(context->scopes.back());
        navigator_.push(std::make_unique<LayeredEditor>(layered_, navigator_, chrome_,
                                                        std::move(*context), EditorLook::Page,
                                                        std::move(title)));
        return;
    }
    lanRoms_.opening(id);
    lanProfiles_.opening(id);
    navigator_.push(std::make_unique<SettingsSubPage>(*this, chrome_, id));
}

void SettingsController::setLevel(const std::string& id, int level) {
    if (shelf_.setLevel(id, level)) {
        refresh();
    }
}

void SettingsController::run(const std::string& id, int step) {
    if (shelf_.run(id, step)) {
        refresh();
        return;
    }
    if (id == keyboardId) {
        settings::Settings& v = preferences_.values();
        const auto from = static_cast<std::size_t>(v.keyboardStyle);
        v.keyboardStyle =
            input::allKeyboardStyles[around(from, step, input::allKeyboardStyles.size())];
        changed();
        return;
    }
    if (id.starts_with(shortcutId) || id == shortcutsResetId) {
        runControls(id);
        return;
    }
    if (MappingEditor::owns(id)) {
        mappings_.runSetting(id, step);
        refresh();
        return;
    }
    if (ChordSettings::owns(id)) {
        chords_.run(id, step);
        refresh();
        return;
    }
    if (IntegrationSettings::applies(id)) {
        applyIntegrations();
        return;
    }
    if (IntegrationSettings::owns(id)) {
        integrations_.run(id, step);
        refresh();
        return;
    }
    if (ArtworkSettings::owns(id)) {
        artwork_.run(id, step);
        refresh();
        return;
    }
    if (InterfaceThemeSettings::owns(id)) {
        interfaceThemes_.run(id, step);
        refresh();
        return;
    }
    if (InterfaceSoundSettings::owns(id)) {
        interfaceSounds_.run(id, step);
        refresh();
        return;
    }
    if (LanSettings::owns(id)) {
        lan_.run(id, step);
        refresh();
        return;
    }
    if (OnlinePlay::owns(id)) {
        if (step == 0) {
            online_.join();
        }
        return;
    }
    if (LanRoms::owns(id)) {
        if (step == 0) {
            lanRoms_.run(id);
        }
        return;
    }
    if (LanProfiles::owns(id)) {
        if (step == 0) {
            lanProfiles_.run(id);
        }
        return;
    }
    if (SaveSettings::owns(id)) {
        saves_.run(id, step);
        refresh();
        return;
    }
    if (ProfileSettings::owns(id)) {
        profiles_.run(id, step);
        refresh();
        return;
    }
    if (RomFolderSettings::owns(id)) {
        romFolders_.run(id, step);
        return;
    }
    if (EmulatorSettings::owns(id)) {
        emulators_.run(id, step);
    } else if (SteamLibrarySettings::owns(id)) {
        steamLibraries_.run(id, step);
    } else if (WineSettings::owns(id)) {
        wine_.run(id, step);
    } else if (PluginSettings::owns(id)) {
        plugins_.run(id, step);
    } else if (id == RomFolderSettings::addId || id == steamAddId || id == gameAddId ||
               id.starts_with(installStoreId)) {
        chooseFolder(id);
    } else if (id == scrollModeId || id == nightLightId || id == soundsId) {
        runAppearance(id, step);
        return;
    } else {
        runLibrary(id, step);
        return;
    }
    refresh();
}

void SettingsController::runAppearance(const std::string& id, int step) {
    settings::Settings& v = preferences_.values();
    if (id == scrollModeId) {
        v.homeMode = interfaceThemes_.homeMode(defaults_.homeMode) == config::HomeMode::WiiSu
                         ? config::HomeMode::Standard
                         : config::HomeMode::WiiSu;
    } else if (id == nightLightId) {
        v.nightLight = settings::allNightLightChoices[around(
            static_cast<std::size_t>(v.nightLight), step, settings::allNightLightChoices.size())];
    } else {
        v.uiSounds = !v.uiSounds;
    }
    changed();
}

void SettingsController::runControls(const std::string& id) {
    if (id == shortcutsResetId) {
        shortcutEditor_.resetAll();
    } else if (const std::optional<input::Action> action = input::actionOf(
                   std::string_view{id}.substr(std::string_view{shortcutId}.size()))) {
        shortcutEditor_.begin(*action);
    }
    refresh();
}

void SettingsController::runLibrary(const std::string& id, int step) {
    settings::Settings& v = preferences_.values();
    if (id == hiddenId) {
        v.hidden = library::HiddenGames{};
        hooks_.reshow();
    } else if (id == emulatorsId) {
        layers_.clearGamePicks();
        hooks_.reloadCatalog();
    } else if (id == gameForgetId) {
        v.gameFolders.clear();
        changed();
        hooks_.libraryFoldersChanged();
        hooks_.say("Game folders forgotten", false);
        return;
    } else if (id == steamResetId) {
        v.steamRoots.clear();
        hooks_.say("Steam is found automatically after a restart", false);
    } else {
        return;
    }
    changed();
}

void SettingsController::addRomFolder() {
    addRoot(std::string{RomFolderSettings::addId});
}

void SettingsController::addGameFolder() {
    addRoot(gameAddId);
}

void SettingsController::chooseFolder(const std::string& id) {
    if (id == RomFolderSettings::addId || id == steamAddId || id == gameAddId) {
        addRoot(id);
    } else if (id == installDefaultId) {
        editInstallFolder("Default install folder", id);
    } else {
        for (const library::Source store : settings::installStores) {
            if (id == storeKeyOf(store)) {
                editInstallFolder(std::string{library::label(store)} + " install folder", id);
            }
        }
    }
}

void SettingsController::addRoot(const std::string& id) {
    const bool steam = id == steamAddId;
    const bool games = id == gameAddId;
    PathEditor::Request request;
    request.title = steam   ? "Add a Steam install folder"
                    : games ? "Add a game folder"
                            : "Add a ROM folder";
    request.start = defaults_.home;
    request.refusal = [steam, this](const fs::path& folder) {
        std::string refused = settings::refusal(folder, settings::Need::Read);
        if (refused.empty() && steam &&
            library::steam::Library::discover(defaults_.home, {folder}).roots().empty()) {
            refused = "that folder holds no Steam installation";
        }
        return refused;
    };
    request.done = [steam, games, this](std::optional<fs::path> folder) {
        if (!folder) {
            return;
        }
        if (!steam && !games) {
            romFolders_.add(*folder);
            return;
        }
        if (games) {
            preferences_.values().addGameFolder(*folder);
            changed();
            hooks_.libraryFoldersChanged();
            hooks_.say("Game folder added", false);
            return;
        }
        std::vector<fs::path>& roots = preferences_.values().steamRoots;
        if (std::ranges::find(roots, *folder) == roots.end()) {
            roots.push_back(*folder);
        }
        hooks_.say("Steam folder saved; it applies after a restart", false);
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

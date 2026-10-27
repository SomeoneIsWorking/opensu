#include "settings_controller.hpp"

#include <algorithm>
#include <memory>

#include "build_version.hpp"
#include "layered_editor.hpp"
#include "library/steam.hpp"
#include "settings/install_folders.hpp"
#include "settings_screen.hpp"
#include "ui/icon_size.hpp"
#include "ui/interface_input.hpp"

namespace opensu::app {
namespace {

namespace fs = std::filesystem;
using gpu2d::ui::Screen;
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
/// What a row reads when the command line or the environment openSU was started with set it.
constexpr const char* startedWith = "Set by how openSU was started";

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
    return static_cast<std::size_t>(
        (static_cast<int>(value) + (step == 0 ? 1 : step) % size + size) % size);
}

/// Every entry of `all` as a choice's options, labelled by `label`, `now` chosen.
template <typename T, std::size_t N, typename Label>
ui::Options optionsOf(const std::array<T, N>& all, T now, Label label) {
    ui::Options options;
    for (std::size_t i = 0; i < N; ++i) {
        options.labels.emplace_back(label(all[i]));
        if (all[i] == now) {
            options.chosen = i;
        }
    }
    return options;
}

/// The first word of `label`: what a question's button does ("Remove", "Clear").
std::string verbOf(const std::string& label) {
    return label.substr(0, label.find(' '));
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
    pages.push_back(aboutPage());
    return pages;
}

ui::SettingsCategory SettingsController::appearancePage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"appearance", "Appearance", {}};
    page.rows.push_back(ui::makeChoice(
        layoutId, "Library layout", "How Library lays out its tiles",
        optionsOf(library::allLibraryModes, v.libraryMode, [](library::LibraryMode mode) {
            return std::string{library::label(mode)};
        })));
    page.rows.push_back(
        ui::makeSlider(iconSizeId, "Icon size", "How large the shelf's tiles are",
                       ui::SliderRange{v.iconSize, ui::minIconLevel, ui::maxIconLevel}));
    const config::HomeMode mode = v.homeMode.value_or(defaults_.homeMode);
    page.rows.push_back(ui::makeChoice(
        scrollModeId, "Scroll mode", "How the home grid scrolls",
        ui::Options{{"Scrolling", "Pages"}, mode == config::HomeMode::WiiSu ? 1U : 0U}));
    page.rows.push_back(ui::makeToggle(pinDockId, "Pin navigation bar on Library",
                                       "Keep the dock up on Library", v.pinLibraryDock));
    ui::Options scales;
    for (int percent = settings::minUiScale; percent <= settings::maxUiScale;
         percent += settings::uiScaleStep) {
        if (percent <= v.uiScale) {
            scales.chosen = scales.labels.size();
        }
        scales.labels.push_back(percentOf(percent));
    }
    page.rows.push_back(ui::makeChoice(uiScaleId, "Interface size", "Scales everything on screen",
                                       std::move(scales)));
    return page;
}

ui::SettingsCategory SettingsController::libraryPage() const {
    const settings::Settings& v = preferences_.values();
    ui::SettingsCategory page{"library", "Library", {}};
    page.rows.push_back(ui::makeHeading("What shows"));
    page.rows.push_back(
        ui::makeChoice(sortId, "Sort by", "How games are ordered",
                       optionsOf(library::allSortKeys, v.view.sort, [](library::SortKey key) {
                           return std::string{library::label(key)};
                       })));
    ui::Options sources;
    for (const library::SourceChoice& choice : hooks_.sourceChoices()) {
        if (choice.key == v.view.source) {
            sources.chosen = sources.labels.size();
        }
        sources.labels.push_back(choice.label);
    }
    page.rows.push_back(
        ui::makeChoice(sourceId, "Show", "Which store's games show", std::move(sources)));
    page.rows.push_back(ui::makeToggle(installedId, "Installed games only",
                                       "Hide games you have not installed", v.view.installedOnly));
    ui::SettingsRow hidden = ui::makeRow(
        hiddenId, RowKind::Action, "Show hidden games again", "Brings back every game you hid",
        v.hidden.keys().empty() ? "None hidden" : countOf(v.hidden.keys().size(), "game"));
    page.rows.push_back(std::move(hidden));
    ui::SettingsRow picks = ui::makeRow(
        emulatorsId, RowKind::Action, "Reset emulator choices",
        "Every ROM goes back to its default emulator",
        layers_.gamePicks() == 0 ? "None chosen" : countOf(layers_.gamePicks(), "game"));
    if (layers_.gamePicks() > 0) {
        picks.confirm = "Every game's own emulator is forgotten; each console's stays.";
    }
    page.rows.push_back(std::move(picks));
    page.rows.push_back(ui::makeHeading("ROM folders"));
    page.rows.push_back(ui::makeRow(
        "roms.list", RowKind::Info, "ROM folders",
        v.romFolders.empty() && !defaults_.romRoots.empty() ? startedWith
                                                            : "Applies after a restart",
        listed(v.romFolders.empty() ? defaults_.romRoots : v.romFolders, "Found automatically")));
    page.rows.push_back(ui::makeRow(romAddId, RowKind::Folder, "Add a ROM folder",
                                    "A folder with one subfolder per system", ""));
    page.rows.push_back(ui::makeRow(romResetId, RowKind::Action, "Find ROM folders automatically",
                                    "Forgets the folders you added", ""));
    if (!inForce_.current().lists(library::Source::Steam)) {
        return page;
    }
    page.rows.push_back(ui::makeHeading("Steam"));
    page.rows.push_back(ui::makeRow(
        "steam.list", RowKind::Info, "Steam install folders",
        v.steamRoots.empty() && !defaults_.steamRoots.empty() ? startedWith
                                                              : "Applies after a restart",
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
    const audio::SystemVolume& volume = volume_.system();
    if (!volume.available()) {
        page.rows.push_back(ui::makeRow("volume.missing", RowKind::Info, "System volume",
                                        volume.unavailable(), "Not available"));
        return page;
    }
    const audio::VolumeState state = volume.state().value_or(audio::VolumeState{});
    page.rows.push_back(ui::makeSlider(volumeId, "System volume",
                                       "The output volume of this computer",
                                       ui::SliderRange{state.percent, 0, 100, audio::volumeStep}));
    page.rows.push_back(ui::makeToggle(muteId, "Mute", "Silences the output", state.muted));
    return page;
}

ui::SettingsCategory SettingsController::controlsPage() const {
    ui::SettingsCategory page{"controls", "Controls", {}};
    page.rows.push_back(ui::makeHeading("Controllers"));
    std::ranges::move(mappings_.settingsRows(), std::back_inserter(page.rows));
    page.rows.push_back(
        ui::makeChoice(keyboardId, "Keyboard", "The on-screen keyboard the fields open",
                       optionsOf(input::allKeyboardStyles, preferences_.values().keyboardStyle,
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
    return {"about",
            "About",
            {ui::makeRow("about.version", RowKind::Info, "Version", "This build of openSU",
                         buildVersion.empty() ? "Unknown" : std::string{buildVersion}),
             ui::makeHeading("Where openSU keeps things"),
             ui::makeRow("about.settings", RowKind::Info, "Settings file", "What you choose here",
                         (defaults_.configDir / "settings.json").string()),
             ui::makeRow("about.data", RowKind::Info, "Data folder", "Sign-ins and GOG installs",
                         defaults_.dataDir.string()),
             ui::makeRow("about.cache", RowKind::Info, "Cache folder", "Artwork and sounds",
                         defaults_.cacheDir.string())}};
}

void SettingsController::open() {
    show({});
}

void SettingsController::openCategory(const std::string& id) {
    show(id);
}

void SettingsController::show(const std::string& category) {
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
        navigator_.clear();
    }
    const std::vector<ui::SettingsCategory>& all = categories();
    const bool known = std::ranges::any_of(all, [&category](const ui::SettingsCategory& page) {
        return page.id == category;
    });
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    navigator_.push(
        std::make_unique<SettingsScreen>(*this, chrome_, known ? category : all.front().id));
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
    navigator_.clear();
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
        countOf(waiting, "change") + " on Integrations " + (waiting == 1 ? "is" : "are") +
            " not applied yet. Discarded changes are lost.",
        std::move(choices)));
    return Screen::BackReply::Stay;
}

void SettingsController::applyIntegrations() {
    const std::size_t applied = integrations_.apply();
    if (applied > 0) {
        hooks_.say("Integrations: " + countOf(applied, "change") + " applied", false);
    }
    refresh();
}

void SettingsController::act(gamepad::Button button) {
    if (shortcutEditor_.capturing()) {
        return;
    }
    if (const std::optional<gpu2d::ui::Action> action = ui::interfaceActionOf(button)) {
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
        while (navigator_.top() != nullptr &&
               dynamic_cast<const SettingsScreen*>(navigator_.top()) == nullptr) {
            navigator_.pop();
        }
        root = dynamic_cast<const SettingsScreen*>(navigator_.top());
    }
    if (root != nullptr && navigator_.focus(root->tabId())) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void SettingsController::confirm(const ui::SettingsRow& row) {
    navigator_.push(
        gpu2d::ui::confirm(row.label + "?", row.confirm, verbOf(row.label), [this, id = row.id] {
            run(id, 0);
        }));
}

void SettingsController::openPage(const std::string& id) {
    if (std::optional<EditorContext> context = layers_.contextOf(id)) {
        std::string title = layered_.scopeName(context->scopes.back());
        navigator_.push(std::make_unique<LayeredEditor>(layered_, navigator_, chrome_,
                                                        std::move(*context), EditorLook::Page,
                                                        std::move(title)));
        return;
    }
    navigator_.push(std::make_unique<SettingsSubPage>(*this, chrome_, id));
}

void SettingsController::setLevel(const std::string& id, int level) {
    if (id == volumeId) {
        if (const std::string refused = volume_.system().setPercent(std::clamp(level, 0, 100));
            !refused.empty()) {
            hooks_.say(refused, true);
        }
        refresh();
        return;
    }
    const int next = std::clamp(level, ui::minIconLevel, ui::maxIconLevel);
    if (id == iconSizeId && next != preferences_.values().iconSize) {
        preferences_.values().iconSize = next;
        changed();
    }
}

void SettingsController::run(const std::string& id, int step) {
    if (id == muteId) {
        runAudio(id, step);
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
    if (EmulatorSettings::owns(id)) {
        emulators_.run(id, step);
    } else if (SteamLibrarySettings::owns(id)) {
        steamLibraries_.run(id, step);
    } else if (WineSettings::owns(id)) {
        wine_.run(id, step);
    } else if (PluginSettings::owns(id)) {
        plugins_.run(id, step);
    } else if (id == romAddId || id == steamAddId || id.starts_with(installStoreId)) {
        chooseFolder(id);
    } else if (id == layoutId || id == scrollModeId || id == pinDockId || id == soundsId ||
               id == uiScaleId) {
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
    if (id == muteId && step == 0) {
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
        shortcutEditor_.begin(*action);
    }
    refresh();
}

void SettingsController::runLibrary(const std::string& id, int step) {
    settings::Settings& v = preferences_.values();
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
    } else {
        return;
    }
    changed();
}

void SettingsController::chooseFolder(const std::string& id) {
    if (id == romAddId || id == steamAddId) {
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

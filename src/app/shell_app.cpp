#include "shell_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <utility>

#include "artwork/iisu_assets.hpp"
#include "config/config.hpp"
#include "frame_png.hpp"
#include "input/keyboard_bindings.hpp"
#include "launcher_status.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "ui/clock_text.hpp"
#include "ui/section_view.hpp"

namespace opensu::app {
namespace {

/// The APK files opensu keeps: every UI sound and the dock's icons.
std::vector<artwork::ApkAsset> iisuAssets() {
    std::vector<artwork::ApkAsset> assets;
    assets.reserve(audio::allEffects.size() + artwork::allNavIcons.size());
    for (const audio::Effect effect : audio::allEffects) {
        assets.push_back(artwork::soundAsset(effect));
    }
    for (const artwork::NavIcon icon : artwork::allNavIcons) {
        assets.push_back(artwork::navAsset(icon));
    }
    return assets;
}

} // namespace

ShellApp::ShellApp(const Settings& settings, const config::Config& environment)
    : environment_{environment}, settings_{settings},
      catalogLoader_{library::makeCatalog(resolved_)},
      shell_{settings_.width, settings_.height, settings_.homeMode, environment_.assetsDir},
      steam_{steam::Client::Options{resolved_.home, resolved_.executablePath, resolved_.session,
                                    resolved_.steamRoots}},
      gameWindows_{environment_.insideGamescope ? std::make_unique<session::GamescopeWindows>()
                                                : nullptr},
      handoff_{environment_.executablePath, environment_.session, steam_, gameWindows_.get()},
      signIn_{StoreSignIn::Options{.dataDir = environment_.dataDir}} {
    shell_.shortcuts() = input::Shortcuts{preferences_.values().shortcuts};
    refreshClock();
    applyLayout();
}

void ShellApp::applyLayout() {
    const settings::Settings& chosen = preferences_.values();
    if (shell_.libraryMode() != chosen.libraryMode) {
        shell_.setLibraryMode(chosen.libraryMode);
    }
    shell_.setPinLibraryDock(chosen.pinLibraryDock);
    if (shell_.iconSize() != chosen.iconSize) {
        shell_.setIconSize(chosen.iconSize);
    }
    shell_.setHomeMode(chosen.homeMode.value_or(settings_.homeMode));
    shell_.setUiScale(chosen.uiScale);
    sounds_.setMuted(!chosen.uiSounds);
}

std::vector<std::filesystem::path> ShellApp::steamLibraries() const {
    std::vector<std::filesystem::path> folders;
    for (const library::steam::LibraryFolder& folder :
         library::steam::Library::discover(resolved_.home, resolved_.steamRoots).libraryFolders()) {
        folders.push_back(folder.path);
    }
    return folders;
}

std::vector<ui::SearchResult> ShellApp::searchResults() const {
    std::vector<ui::SearchResult> results;
    for (const ui::Tile& tile : shell_.tiles()) {
        const auto* game = std::get_if<library::Game>(&tile.item);
        results.push_back(ui::SearchResult{game != nullptr ? game->title : tile.title,
                                           library::describeResult(tile.item)});
    }
    return results;
}

void ShellApp::openTile(std::size_t index) {
    shell_.focusTile(index);
    actOn(gamepad::Button::A);
}

std::filesystem::path ShellApp::padsDirectory(const std::filesystem::path& dataDir, bool hidden) {
    if (!hidden) {
        return "/dev/input";
    }
    const std::filesystem::path empty = dataDir / "hidden-pads";
    std::filesystem::create_directories(empty);
    return empty;
}

void ShellApp::reloadCatalog() {
    catalogLoader_.refresh();
}

void ShellApp::serviceCatalog() {
    if (std::optional<library::CatalogSnapshot> snapshot = catalogLoader_.take()) {
        applyCatalog(std::move(*snapshot));
    }
}

void ShellApp::loadCatalogNow() {
    catalogLoader_.refresh();
    catalogLoader_.waitIdle();
    serviceCatalog();
}

void ShellApp::applyCatalog(library::CatalogSnapshot snapshot) {
    games_ = std::move(snapshot.games);
    sources_ = std::move(snapshot.sources);
    preferences_.values().lastPlayed.apply(games_);
    preferences_.values().emulators.apply(games_);
    artworkStore_.apply(games_);
    const std::vector<library::Console> consoles = library::consoles(games_);
    artworkFetcher_.request(games_, consoles, iisuAssets());
    delivery_.restart();
    for (const library::Console& console : consoles) {
        const std::filesystem::path glyph = artworkStore_.storedGlyph(console.system);
        if (!glyph.empty()) {
            shell_.setGlyph(console.system, glyph);
        }
    }
    for (const library::SourceStatus& source : sources_) {
        if (source.availability != library::Availability::Ready &&
            source.availability != library::Availability::Loading) {
            lucent::warn("catalog", "{}: {}", library::label(source.source), source.detail);
        }
    }
    lucent::info("catalog", "{} games loaded", games_.size());

    installedCount_ =
        static_cast<std::size_t>(std::ranges::count_if(games_, &library::Game::installed));
    showShelf(shell_.focusIndex());
}

void ShellApp::pushCatalogToShell() {
    showShelf(shell_.focusIndex());
}

void ShellApp::showShelf(std::size_t focus) {
    const settings::Settings& chosen = preferences_.values();
    std::vector<library::ShelfItem> shelf =
        library::visibleShelf(browser_, games_, sources_, chosen.view, chosen.hidden);
    artworkStore_.apply(shelf);
    shell_.setShelf(std::move(shelf), focus);
    shell_.setHeader(folderCard());
    shell_.setStatus(std::to_string(games_.size()) + " games · " + std::to_string(installedCount_) +
                     " installed" + (library::narrowing(chosen.view) ? " · filtered" : ""));
    shell_.setArtworkDownloading(artworkFetcher_.pendingGames());
    details_.refresh();
}

std::optional<library::ShelfItem> ShellApp::folderCard() const {
    const std::optional<library::Folder> folder = browser_.folder();
    if (!folder || search_.searching()) {
        return std::nullopt;
    }
    std::vector<library::ShelfItem> card{std::visit(
        [](const auto& opened) {
            return library::ShelfItem{opened};
        },
        *folder)};
    artworkStore_.apply(card);
    return std::move(card.front());
}

void ShellApp::openFolder(const library::Folder& folder) {
    if (const auto* launcher = std::get_if<library::Launcher>(&folder);
        launcher != nullptr && launcher->games == 0) {
        const std::string store{library::label(launcher->source)};
        if (launcher->loading) {
            shell_.setToast("still loading " + store);
            return;
        }
        if (!launcher->ready && launcher->source == library::Source::Gog) {
            startSignIn(Store::Gog);
            return;
        }
        if (!launcher->ready && launcher->source == library::Source::Epic) {
            startSignIn(Store::Epic);
            return;
        }
        shell_.setToast(launcher->ready ? "no games in " + store : "sign in to " + store, true);
        return;
    }
    // input-sound.md 3.4 EnterConsolesApps: A on a console, an app category or a collection.
    sounds_.play(audio::Effect::EnterConsolesApps);
    // A folder opened from search results is entered from the top of its section's shelf.
    const std::size_t from = search_.searching() ? 0 : shell_.focusIndex();
    search_.clear();
    browser_.open(folder, from);
    showShelf(0);
}

void ShellApp::startSignIn(Store store) {
    shell_.setToast("opening the sign-in page in your browser");
    // The opener can take seconds to start a browser, so it runs off the loop.
    signInOpener_ = std::jthread{[this, store] {
        const SignInResult result = signIn_.open(store);
        requestToast(result.ok ? "sign in there; the opensu sign-in extension finishes it"
                               : result.message,
                     !result.ok);
    }};
}

void ShellApp::handleEvents(const std::vector<gamepad::Event>& incoming) {
    if (shortcutEditor_.capturing()) {
        shortcutEditor_.capturePad(incoming);
        return;
    }
    const std::vector<gamepad::Event> events = router_.fromPad(incoming);
    for (const gamepad::Event& event : events) {
        switch (event.kind) {
        case gamepad::Event::Kind::Connected:
            lucent::info("gamepad", "controller ready: {}", event.device);
            break;
        case gamepad::Event::Kind::Disconnected:
            shell_.setToast("controller disconnected", true);
            break;
        case gamepad::Event::Kind::Button:
            if (gamepad::DirectionRepeat::repeats(event.button)) {
                if (event.pressed) {
                    repeat_.press(event.button, std::chrono::steady_clock::now());
                } else {
                    repeat_.release(event.button);
                }
            } else if (event.pressed) {
                actOn(event.button);
            }
            break;
        }
    }
}

/// Keyboard input, so the shell is usable without a controller. Every key maps
/// to the button it stands in for rather than to a shell command of its own, so
/// there is one set of actions and the keyboard is a second way to reach it.
void ShellApp::handleKeyboard() {
    // Typed characters are read first: the key that opens the search must not also type itself.
    const bool searching = shell_.searchPanel().isOpen();
    const bool typingPath = paths_.typing();
    const bool typingPassword = sessionInstall_.typing();
    const input::TextInput typed = input::readTextInput();
    if (searching) {
        handleSearchText(typed);
        return;
    }
    if (typingPath) {
        typeInto(paths_, typed);
        return;
    }
    if (typingPassword) {
        typeInto(sessionInstall_, typed);
        return;
    }
    if (shortcutEditor_.capturing()) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            shortcutEditor_.cancel();
        } else if (const std::optional<input::Combo> combo =
                       input::pressedCombo(input::raylibKeys())) {
            shell_.inputDevice().noteKey();
            shortcutEditor_.captureKey(*combo);
        }
        return;
    }
    const ShortcutRouter::Keys keys = router_.fromKeys(input::raylibKeys());
    if (keys.any) {
        shell_.inputDevice().noteKey();
    }
    handleEvents(keys.events);
}

void ShellApp::handleSearchText(const input::TextInput& typed) {
    if (!typed.any()) {
        return;
    }
    shell_.inputDevice().noteKey();
    search_.typeText(typed.text);
    if (typed.backspace) {
        search_.backspace();
    }
    if (typed.up) {
        search_.walk(ui::Direction::Up);
    }
    if (typed.down) {
        search_.walk(ui::Direction::Down);
    }
    if (typed.enter) {
        search_.confirm();
    } else if (typed.escape) {
        search_.dismiss();
    }
}

template <class Entry> void ShellApp::typeInto(Entry& entry, const input::TextInput& typed) {
    if (!typed.any()) {
        return;
    }
    shell_.inputDevice().noteKey();
    entry.typeText(typed.text);
    if (typed.backspace) {
        entry.backspace();
    }
    if (typed.enter) {
        entry.confirm();
    } else if (typed.escape) {
        entry.dismiss();
    }
}

void ShellApp::handlePads() {
    const std::vector<gamepad::Event> events = pads_.takeEvents();
    for (const gamepad::Event& event : events) {
        shell_.inputDevice().notePad(event);
        if (roster_.note(event)) {
            devicesScreen_.controllerChanged();
        }
    }
    handleEvents(events);
}

void ShellApp::handlePointer() {
    pointer_.route(readPointerFrame());
}

ui::PointerTarget ShellApp::pointAt(std::optional<Vector2> point) {
    return shell_.pointAt(point);
}

void ShellApp::focus(const ui::PointerTarget& target) {
    // input-sound.md 3.4 Navigation: a focus move.
    if (shell_.focusTarget(target)) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void ShellApp::press(gamepad::Button button) {
    actOn(button);
}

void ShellApp::activateSection(library::Section section) {
    clickSection(section);
}

void ShellApp::selectLauncher(library::Source source) {
    // The gates actOn puts in front of every button.
    if (shell_.inGame() || panels_.active() || shell_.panelOpen()) {
        return;
    }
    // Selecting the store's tile in Library: from its top, whatever is open now.
    search_.clear();
    if (browser_.section() != library::Section::Library) {
        clickSection(library::Section::Library);
    } else if (const std::optional<std::size_t> focus = browser_.back()) {
        showShelf(*focus);
    }
    const std::vector<ui::Tile>& tiles = shell_.tiles();
    const auto tile = std::ranges::find_if(tiles, [source](const ui::Tile& candidate) {
        const auto* launcher = std::get_if<library::Launcher>(&candidate.item);
        return launcher != nullptr && launcher->source == source;
    });
    if (tile == tiles.end()) {
        shell_.setToast("no " + std::string{library::label(source)} + " library here", true);
        return;
    }
    focus(ui::OnTile{static_cast<std::size_t>(tile - tiles.begin())});
    actOn(gamepad::Button::A);
}

void ShellApp::scroll(int steps) {
    const bool modal = shell_.inGame() || panels_.active() || shell_.panelOpen();
    if (!modal && shell_.layout().mode() == ui::ScrollMode::Paged &&
        shell_.presentation() == ui::Presentation::Grid) {
        focus(ui::OnPage{shell_.page() + steps});
        return;
    }
    // The Guide menu and an XMB run down the screen; the rest run across it.
    const bool vertical = shell_.inGame() || shell_.panelOpen() ||
                          (!modal && shell_.presentation() == ui::Presentation::Xmb);
    if (vertical) {
        actOn(steps > 0 ? gamepad::Button::Down : gamepad::Button::Up);
    } else {
        actOn(steps > 0 ? gamepad::Button::Right : gamepad::Button::Left);
    }
}

void ShellApp::clickSection(library::Section section) {
    // The gates actOn puts in front of L1 and R1.
    if (shell_.inGame() || panels_.active() || shell_.panelOpen()) {
        return;
    }
    const int steps = library::Sections::stepsBetween(browser_.section(), section);
    if (steps != 0) {
        cycleSection(steps);
    }
}

void ShellApp::handleGameKeys() {
    if (session::GameKeys* keys = gameScreen_.keys()) {
        handleEvents(router_.fromGame(*keys));
    }
}

void ShellApp::actOn(gamepad::Button button) {
    if (button == gamepad::Button::Guide) {
        toggleGuide();
        return;
    }
    if (shell_.guidePanels().guide().isOpen()) {
        guideMenu_.act(button);
        return;
    }
    if (shell_.guidePanels().quick().isOpen()) {
        quickMenu_.act(button);
        return;
    }
    if (sessionInstall_.active()) {
        sessionInstall_.act(button);
        return;
    }
    // Over a running game only the pages the Guide menu opened take buttons.
    if (shell_.inGame() && !shell_.panelOpen()) {
        return;
    }
    if (panels_.active()) {
        panels_.act(button);
        return;
    }
    if (shell_.contextMenu().isOpen()) {
        contextMenu_.act(button);
        return;
    }
    if (shell_.searchPanel().isOpen()) {
        search_.act(button);
        return;
    }
    if (shell_.modeChooser().isOpen()) {
        layoutPicker_.act(button);
        return;
    }
    if (paths_.active()) {
        paths_.act(button);
        return;
    }
    if (shell_.devicesPanel().isOpen()) {
        devicesScreen_.act(button);
        return;
    }
    if (shell_.settingsPanels().page().isOpen()) {
        settingsScreen_.act(button);
        return;
    }
    if (shell_.detailsPage().isOpen()) {
        details_.act(button);
        return;
    }
    switch (button) {
    case gamepad::Button::Up:
        moveFocus(ui::Direction::Up);
        break;
    case gamepad::Button::Down:
        moveFocus(ui::Direction::Down);
        break;
    case gamepad::Button::Left:
        moveFocus(ui::Direction::Left);
        break;
    case gamepad::Button::Right:
        moveFocus(ui::Direction::Right);
        break;
    case gamepad::Button::A:
        shell_.pressFocused();
        if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
            openFolder(*folder);
        } else {
            details_.open();
        }
        break;
    case gamepad::Button::Y:
        details_.open();
        break;
    case gamepad::Button::Select:
        // iiSU `pb0.java:2086`: SELECT asks for the focused item's menu.
        contextMenu_.open();
        break;
    case gamepad::Button::Search:
        search_.open();
        break;
    case gamepad::Button::X:
        reloadCatalog();
        shell_.setToast("library refreshed");
        break;
    case gamepad::Button::L1:
        cycleSection(-1);
        break;
    case gamepad::Button::R1:
        cycleSection(1);
        break;
    case gamepad::Button::Start:
        layoutPicker_.open(browser_.section() == library::Section::Library);
        break;
    case gamepad::Button::B:
        if (search_.clear()) {
            // input-sound.md 3.4 ExitConsolesApps: back out of what was opened.
            sounds_.play(audio::Effect::ExitConsolesApps);
        } else if (const std::optional<std::size_t> focus = browser_.back()) {
            // input-sound.md 3.4 ExitConsolesApps: back out of a console, an app category or a
            // collection.
            sounds_.play(audio::Effect::ExitConsolesApps);
            showShelf(*focus);
        }
        break;
    default:
        break;
    }
}

void ShellApp::moveFocus(ui::Direction direction) {
    // input-sound.md 3.4 Navigation: a D-pad focus move in the home grid.
    if (shell_.moveFocus(direction)) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void ShellApp::cycleSection(int delta) {
    // Search results belong to no section; moving along the dock ends the search.
    if (!search_.clear()) {
        browser_.leave(shell_.focusIndex());
    }
    const std::size_t focus = browser_.cycle(delta);
    shell_.setSection(browser_.section(), true);
    showShelf(focus);
    // input-sound.md 3.3 (`dg3.a`): the cue is sized by how many tiles the section shows at once.
    const std::size_t visible = ui::visibleTiles(
        browser_.section(), preferences_.values().libraryMode, shell_.tiles().size());
    if (const std::optional<audio::Effect> cue = audio::dominoFor(visible)) {
        sounds_.play(*cue);
    }
}

void ShellApp::launch(const library::Game& game) {
    if (!game.installed) {
        offerInstall(game);
        return;
    }
    if (game.launch.empty()) {
        shell_.setToast(
            game.unavailable.empty() ? "no way to start " + game.title : game.unavailable, true);
        return;
    }
    {
        const std::lock_guard lock{launchMutex_};
        if (launchRunning_) {
            shell_.setToast("a game is already running", true);
            return;
        }
        launchRunning_ = true;
    }

    // input-sound.md 3.4 OpenAppRom: every game launch.
    sounds_.play(audio::Effect::OpenAppRom);
    runningTitle_ = game.title;
    panels_.showLaunch(game.title);
    recordLaunch(game);

    std::vector<std::string> environment = pads_.hold();
    padsHeld_ = true;

    // The thread owns a copy because it outlives this call.
    std::thread{[this, copy = game, environment = std::move(environment)] {
        std::string failure;
        // raylib's window calls belong to the thread that owns the GL context, so the handoff
        // thread only raises flags and the loop does the work.
        const launch::Handoff::Hooks hooks{.hide =
                                               [this] {
                                                   requestGameRunning(true);
                                               },
                                           .show =
                                               [this] {
                                                   requestGameRunning(false);
                                               },
                                           .progress =
                                               [this](const launch::LaunchProgress& progress) {
                                                   requestLaunchProgress(progress);
                                               }};
        handoff_.start(copy, hooks, environment, failure);
        {
            const std::lock_guard lock{launchMutex_};
            launchRunning_ = false;
        }
        if (!failure.empty()) {
            lucent::error("launch", "{}", failure);
            requestToast(failure, true);
        } else {
            requestToast(copy.title + " closed", false);
        }
    }}.detach();
}

void ShellApp::offerInstall(const library::Game& game) {
    // A store's own page installs that store's copy; elsewhere any store that owns the title will
    // do.
    panels_.offerInstall(game, browser_.inLauncher() ? std::vector<library::Game>{game}
                                                     : library::copiesOf(games_, game));
}

void ShellApp::cancelLaunch() {
    {
        const std::lock_guard lock{launchMutex_};
        if (!launchRunning_) {
            return;
        }
    }
    lucent::info("launch", "cancelling the launch of {}", runningTitle_);
    handoff_.forceClose();
}

void ShellApp::chooseLevel(int level) {
    if (shell_.guidePanels().quick().isOpen()) {
        quickMenu_.chooseLevel(level);
    } else if (shell_.modeChooser().isOpen()) {
        layoutPicker_.chooseIconSize(level);
    } else if (shell_.devicesPanel().isOpen()) {
        devicesScreen_.chooseLevel(level);
    } else if (shell_.settingsPanels().onlyPage()) {
        settingsScreen_.chooseLevel(level);
    }
}

bool ShellApp::menusBlocked() {
    return panels_.active() || paths_.active() || sessionInstall_.active() ||
           shortcutEditor_.capturing() || shell_.searchPanel().isOpen() ||
           shell_.modeChooser().isOpen() || shell_.contextMenu().isOpen();
}

void ShellApp::closeRunningGame() {
    lucent::info("launch", "closing {} from a menu", runningTitle_);
    handoff_.forceClose();
}

void ShellApp::toggleGuide() {
    ui::GuidePanels& menus = shell_.guidePanels();
    if (menus.quick().isOpen()) {
        quickMenu_.close();
    } else if (menus.guide().isOpen() || !menusBlocked()) {
        guideMenu_.toggle();
    }
}

void ShellApp::toggleQuickMenu() {
    ui::GuidePanels& menus = shell_.guidePanels();
    if (menus.guide().isOpen()) {
        guideMenu_.close();
    }
    if (menus.quick().isOpen() || !menusBlocked()) {
        quickMenu_.toggle();
    }
}

void ShellApp::closePages() {
    details_.close();
    settingsScreen_.close();
    devicesScreen_.close();
}

void ShellApp::syncOverlay() {
    const bool wanted = shell_.inGame() && shell_.panelOpen();
    if (gameScreen_.setShown(wanted) && padsHeld_) {
        pads_.setBlocked(wanted);
    }
}

std::string ShellApp::outputLine() {
    const int monitor = GetCurrentMonitor();
    return std::to_string(GetMonitorWidth(monitor)) + " x " +
           std::to_string(GetMonitorHeight(monitor)) + " at " +
           std::to_string(GetMonitorRefreshRate(monitor)) + " Hz";
}

void ShellApp::serviceDevices(std::chrono::steady_clock::time_point now) {
    const bool quickOpen = shell_.guidePanels().quick().isOpen();
    if (bluetooth_.poll(now, devicesScreen_.showsBluetooth() || quickOpen)) {
        devicesScreen_.refresh();
        quickMenu_.refresh();
    }
    if (const std::optional<BluetoothNotice> notice = bluetooth_.takeNotice()) {
        shell_.setToast(notice->text, notice->error);
    }
    devicesScreen_.tick(now);
    quickMenu_.tick(now);
}

void ShellApp::contextMenu(const ui::PointerTarget& target) {
    if (shell_.inGame() || panels_.active() || shell_.guidePanels().anyOpen() ||
        sessionInstall_.active()) {
        return;
    }
    if (shell_.devicesPanel().isOpen()) {
        // A right click on a Bluetooth device forgets it, as Select does.
        if (std::holds_alternative<ui::OnSettingsRow>(target)) {
            focus(target);
            devicesScreen_.forgetFocused();
        }
        return;
    }
    // Right click dismisses an open menu from outside it, and opens a tile's menu; it is never
    // Back.
    if (shell_.contextMenu().isOpen()) {
        if (std::holds_alternative<ui::OnBackdrop>(target)) {
            contextMenu_.close();
        }
        return;
    }
    if (shell_.panelOpen() || !std::holds_alternative<ui::OnTile>(target)) {
        return;
    }
    focus(target);
    contextMenu_.open();
}

void ShellApp::setHidden(const library::Game& game, bool hidden) {
    preferences_.values().hidden.set(game, hidden);
    preferences_.save();
    showShelf(shell_.focusIndex());
    shell_.setToast(game.title + (hidden ? " hidden" : " shown again"));
}

void ShellApp::recordLaunch(const library::Game& game) {
    preferences_.values().lastPlayed.record(game.id, std::chrono::system_clock::now());
    preferences_.values().lastPlayed.apply(games_);
    preferences_.save();
}

void ShellApp::chooseEmulator(const library::Game& game, const std::string& name) {
    preferences_.values().emulators.set(game.id, name);
    preferences_.save();
    preferences_.values().emulators.apply(games_);
    showShelf(shell_.focusIndex());
}

std::optional<library::Game> ShellApp::shownGame(const std::string& id) const {
    for (const ui::Tile& tile : shell_.tiles()) {
        const auto* game = std::get_if<library::Game>(&tile.item);
        if (game == nullptr || game->id != id) {
            continue;
        }
        std::vector<library::Game> shown{*game};
        preferences_.values().lastPlayed.apply(shown);
        return std::move(shown.front());
    }
    return std::nullopt;
}

ui::Trail ShellApp::currentTrail() const {
    const settings::Settings& chosen = preferences_.values();
    TrailState state{.section = browser_.section(), .folder = browser_.folder()};
    if (search_.searching()) {
        state.search = chosen.view.search;
    }
    if (!chosen.view.source.empty()) {
        state.filters = filtersOf(chosen.view, library::sourceChoices(games_, sources_));
    } else {
        state.filters = filtersOf(chosen.view, {});
    }
    if (shell_.detailsPage().isOpen()) {
        state.game = shell_.detailsPage().view().title;
    }
    state.settings = settingsScreen_.category();
    state.devices = devicesScreen_.tab();
    return trailOf(state);
}

void ShellApp::activateCrumb(std::size_t index) {
    const ui::Trail trail = currentTrail();
    if (index >= trail.size() || !ui::isPlace(trail[index]) || shell_.inGame() ||
        panels_.active()) {
        return;
    }
    // The details page, the Settings screen and the Devices page are the only panels the trail
    // stays clickable over.
    details_.close();
    if (shell_.panelOpen() && !shell_.settingsPanels().onlyPage() &&
        !shell_.devicesPanel().isOpen()) {
        return;
    }
    const ui::Crumb& crumb = trail[index];
    if (crumb.kind == ui::CrumbKind::Devices) {
        devicesScreen_.showTabs();
    } else if (crumb.kind == ui::CrumbKind::Settings) {
        settingsScreen_.showCategories();
    } else if (crumb.kind == ui::CrumbKind::Section && crumb.section != browser_.section()) {
        clickSection(crumb.section);
    } else if (crumb.kind != ui::CrumbKind::Search) {
        search_.clear();
        if (crumb.kind == ui::CrumbKind::Section) {
            if (const std::optional<std::size_t> focus = browser_.back()) {
                showShelf(*focus);
            }
        }
    }
}

ui::HintContext ShellApp::hints() const {
    // The menus draw their own prompts.
    if (paths_.active() || sessionInstall_.active() || shell_.guidePanels().anyOpen()) {
        return ui::HintContext{};
    }
    if (shortcutEditor_.capturing()) {
        return ui::HintContext{.back = true};
    }
    if (shell_.devicesPanel().isOpen()) {
        return ui::HintContext{.back = true,
                               .select = true,
                               .change = devicesScreen_.changes(),
                               .forget = devicesScreen_.forgets()};
    }
    if (shell_.settingsPanels().page().isOpen()) {
        return ui::HintContext{.back = true, .select = settingsScreen_.changes(), .change = true};
    }
    if (shell_.detailsPage().isOpen()) {
        return ui::HintContext{.back = true, .select = true};
    }
    const bool game = shell_.focusedGame() != nullptr;
    const bool folder = shell_.focusedFolder().has_value();
    return ui::HintContext{.back = browser_.canBack(),
                           .clearSearch = search_.searching(),
                           .select = game || folder,
                           .details = game,
                           .options = game || folder,
                           .menu = true};
}

void ShellApp::syncChrome() {
    shell_.setHints(hints());
    shell_.setLaunchers(launcherBadges(steam_.state(), steam_.downloads(), sources_));
    // iiSU's Home has no title (pl3.q); opensu's names the focused tile, except over a page, whose
    // trail names where it is.
    shell_.setTitle(shell_.pillTitle());
    shell_.setTrail(currentTrail());
}

void ShellApp::refreshClock() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm parts{};
    localtime_r(&seconds, &parts);
    const auto millisecond = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
        1000);
    shell_.setClock(ui::ClockText::format(parts.tm_hour, parts.tm_min, environment_.clock24Hour));
    nextClockTick_ = std::chrono::steady_clock::now() +
                     ui::ClockText::untilNextMinute(parts.tm_sec, millisecond);
    // STOPGAP: the battery is re-read on the clock's minute tick because opensu has no
    // power_supply uevent listener standing in for iiSU's ACTION_BATTERY_CHANGED receiver.
    shell_.setBattery(battery_.read());
}

void ShellApp::requestGameRunning(bool running) {
    gameRunning_.store(running);
}

void ShellApp::requestLaunchProgress(const launch::LaunchProgress& progress) {
    const std::lock_guard lock{progressMutex_};
    pendingProgress_ = progress;
}

void ShellApp::requestToast(std::string text, bool isError) {
    {
        const std::lock_guard lock{toastMutex_};
        pendingToast_ = std::move(text);
        pendingToastError_ = isError;
        hasPendingToast_ = true;
    }
}

void ShellApp::publishSnapshot() {
    ShellSnapshot next;
    next.games = games_.size();
    next.installed = installedCount_;
    if (const library::Game* focused = shell_.focusedGame(); focused != nullptr) {
        next.focusedId = focused->id;
    } else if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
        next.focusedId = std::holds_alternative<library::Console>(*folder)
                             ? "console:" + library::key(*folder)
                             : library::key(*folder);
    }
    next.focusedTitle = shell_.focusedTitle();
    next.section = std::string{library::key(browser_.section())};
    next.libraryMode = std::string{library::key(preferences_.values().libraryMode)};
    next.modeChooserOpen = shell_.modeChooser().isOpen();
    next.searchOpen = shell_.searchPanel().isOpen();
    next.searchText = preferences_.values().view.search;
    next.contextMenuOpen = shell_.contextMenu().isOpen();
    next.detailsOpen = shell_.detailsPage().isOpen();
    next.settingsOpen = shell_.settingsPanels().page().isOpen();
    next.folderPickerOpen = paths_.active();
    next.capturingShortcut = shortcutEditor_.capturing();
    next.volumeShown = shell_.volumeOsd().holding();
    next.uiScale = preferences_.values().uiScale;
    if (const auto& level = volume_.system().state()) {
        next.volumePercent = level->percent;
        next.volumeMuted = level->muted;
    }
    for (const ui::Crumb& crumb : currentTrail()) {
        next.breadcrumb += (next.breadcrumb.empty() ? "" : " > ") + crumb.label;
    }
    next.iconSize = static_cast<std::size_t>(shell_.iconSize());
    next.shelf = browser_.folder() ? library::key(*browser_.folder()) : next.section;
    next.focusIndex = shell_.focusIndex();
    next.page = static_cast<std::size_t>(std::max(shell_.page(), 0));
    next.pageCount = static_cast<std::size_t>(std::max(shell_.pageCount(), 1));
    next.status = shell_.status();
    next.toast = shell_.toast();
    next.toastIsError = shell_.toastIsError();
    {
        const std::lock_guard lock{launchMutex_};
        next.launching = launchRunning_;
    }
    // What the loop has applied, not what was asked for, so a request that never reached the
    // loop cannot read as in game.
    next.inGame = shell_.inGame();
    next.guideMenuOpen = shell_.guidePanels().guide().isOpen();
    next.quickMenuOpen = shell_.guidePanels().quick().isOpen();
    next.devicesOpen = shell_.devicesPanel().isOpen();
    next.devicesTab = devicesScreen_.tab();
    next.steam = std::string{launch::name(steam_.state())};
    next.inputDevice =
        shell_.inputDevice().current() == input::Device::KeyboardMouse ? "keyboard" : "pad";
    next.launchers = describe(launcherBadges(steam_.state(), steam_.downloads(), sources_));

    bridge_.publish(std::move(next));
}

void ShellApp::serviceControlRequests() {
    // Buttons injected over the channel take the same path as a real press, so
    // what the channel exercises is the shell's own handling.
    const ControlBridge::Input queued = bridge_.takeInput();
    for (const input::Combo& combo : queued.keys) {
        shell_.inputDevice().noteKey();
        if (shortcutEditor_.capturing()) {
            shortcutEditor_.captureKey(combo);
        } else {
            handleEvents(router_.fromCombo(combo));
        }
    }
    for (const std::string& text : queued.text) {
        if (shell_.searchPanel().isOpen()) {
            shell_.inputDevice().noteKey();
            search_.typeText(text);
        } else if (paths_.typing()) {
            shell_.inputDevice().noteKey();
            paths_.typeText(text);
        } else if (sessionInstall_.typing()) {
            sessionInstall_.typeText(text);
        }
    }
    // An injected button is a tap: without its release a direction would repeat forever.
    for (const auto& [button, device] : queued.buttons) {
        if (device == input::Device::KeyboardMouse) {
            shell_.inputDevice().noteKey();
        } else {
            shell_.inputDevice().notePad(gamepad::Event{.button = button, .pressed = true});
        }
        handleEvents({gamepad::Event{
                          .kind = gamepad::Event::Kind::Button, .button = button, .pressed = true},
                      gamepad::Event{.kind = gamepad::Event::Kind::Button,
                                     .button = button,
                                     .pressed = false}});
    }

    if (!bridge_.frameWanted()) {
        return;
    }
    std::string png;
    bridge_.answerFrame(renderShellPng(shell_, png) ? std::move(png) : std::string{});
}

/// Applies what the launch thread and the control channel asked for. Everything
/// that touches the window or the shell happens here, on the loop's thread.
void ShellApp::serviceRequests() {
    bool launching = false;
    {
        const std::lock_guard lock{launchMutex_};
        launching = launchRunning_;
    }
    if (padsHeld_ && !launching) {
        pads_.release();
        padsHeld_ = false;
    }
    std::optional<launch::LaunchProgress> progress;
    {
        const std::lock_guard lock{progressMutex_};
        progress = std::exchange(pendingProgress_, std::nullopt);
    }
    if (progress) {
        const bool measured = progress->stage == launch::LaunchProgress::Stage::Updating;
        shell_.launchPanel().update(launch::describe(*progress),
                                    measured ? std::optional{progress->fraction} : std::nullopt);
    }
    if (!launching) {
        panels_.endLaunch();
    }
    panels_.service();
    if (const std::optional<std::string> reload = bridge_.takeReload()) {
        if (!reload->empty()) {
            shell_.setToast(*reload);
        }
        reloadCatalog();
    }
    const bool running = gameRunning_.load();
    if (running != shell_.inGame()) {
        shell_.setInGame(running);
        guideMenu_.close();
        quickMenu_.close();
        closePages();
        panels_.endLaunch();
        gameScreen_.setRunning(running);
    }

    if (hasPendingToast_) {
        std::string text;
        bool isError = false;
        {
            const std::lock_guard lock{toastMutex_};
            text = std::move(pendingToast_);
            isError = pendingToastError_;
            pendingToast_.clear();
            hasPendingToast_ = false;
        }
        shell_.setToast(std::move(text), isError);
    }
}

int ShellApp::run() {
    reloadCatalog();

    // Resizable, because the layout is computed from the window size rather than
    // baked at 1280x800, and because a fixed window on a scaled desktop is
    // unusable. The scale factor is applied by the window manager on the way in,
    // so what the shell draws in is already in its own pixels.
    // Transparent, so the Guide menu can draw over a game with the game showing through. No
    // MSAA: with it, Gamescope's Xwayland gave a 24-bit window, which it composites as opaque.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT |
                   (settings_.hidden ? FLAG_WINDOW_HIDDEN : 0u));
    InitWindow(settings_.width, settings_.height, "openSU");
    SetWindowMinSize(960, 600);
    shell_.loadFonts();
    if (environment_.insideGamescope && !settings_.hidden) {
        // Gamescope composites a window as the overlay only when it spans the whole screen,
        // which also draws the home screen at the output's own resolution.
        const int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        shell_.setSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        gameScreen_.attachOverlay(*static_cast<const unsigned long*>(GetWindowHandle()));
    }
    sounds_.open();
    delivery_.loadStoredSounds();
    // Textures need a GL context; the shell streams artwork in as frames are drawn.
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(bridge_, signIn_, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background while the shell is already usable. A machine
    // without a Steam install has nothing to start, and shows no Steam icon.
    if (settings_.startSteam &&
        !library::steam::Library::discover(resolved_.home, resolved_.steamRoots).roots().empty()) {
        steam_.start();
    }

    while (!bridge_.closeRequested() && !WindowShouldClose()) {
        // A resize changes the framebuffer, so the layout has to be recomputed
        // before anything is drawn into it. Checked every frame because there is
        // no resize callback worth relying on across platforms.
        if (IsWindowResized()) {
            shell_.setSize(GetScreenWidth(), GetScreenHeight());
        }

        handlePads();
        handleKeyboard();
        handlePointer();
        handleGameKeys();
        volume_.poll(std::chrono::steady_clock::now());
        if (const auto direction = repeat_.poll(std::chrono::steady_clock::now())) {
            actOn(*direction);
        }

        serviceControlRequests();
        serviceRequests();
        serviceCatalog();
        delivery_.service();
        serviceDevices(std::chrono::steady_clock::now());
        sessionInstall_.service();
        syncOverlay();
        syncChrome();
        shell_.tick(std::chrono::steady_clock::now());
        publishSnapshot();
        shell_.draw();

        // iiSU k42: the clock re-renders on the minute boundary.
        if (std::chrono::steady_clock::now() >= nextClockTick_) {
            refreshClock();
        }
    }

    // Anything waiting on a frame will never get one now.
    bridge_.abandonFrame();
    if (control_) {
        control_->stop();
    }
    CloseWindow();
    return 0;
}

bool ShellApp::renderToFile(const std::string& path, bool keyboardPrompts) {
    if (keyboardPrompts) {
        shell_.inputDevice().noteKey();
    }
    if (games_.empty()) {
        loadCatalogNow();
    } else {
        pushCatalogToShell();
    }

    // raylib needs a GL context before any texture work, and a context needs a
    // window. The window is never shown and nothing is presented, so a render
    // still lands on no screen.
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(settings_.width, settings_.height, "opensu render");
    shell_.loadFonts();
    shell_.loadArtworkNow();
    lucent::info("render", "loaded artwork for {} of {} tiles", shell_.loadedArtwork(),
                 shell_.tiles().size());
    // A still frame shows the grid at rest, after its entrance.
    shell_.tick(std::chrono::steady_clock::now());
    syncChrome();
    shell_.settle();

    std::string png;
    const bool ok = renderShellPng(shell_, png);
    CloseWindow();
    if (!ok) {
        lucent::error("render", "could not render a frame");
        return false;
    }

    std::ofstream out{path, std::ios::binary};
    out.write(png.data(), static_cast<std::streamsize>(png.size()));
    const bool written = out.good();
    if (written) {
        lucent::info("render", "wrote {}", path);
    } else {
        lucent::error("render", "could not write {}", path);
    }
    return written;
}

} // namespace opensu::app
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

ShellApp::ShellApp(const Settings& settings)
    : settings_{settings}, catalogLoader_{library::makeCatalog(resolved_)},
      shell_{settings_.width, settings_.height, settings_.homeMode},
      steam_{steam::Client::Options{resolved_.home, resolved_.executablePath, resolved_.session,
                                    resolved_.steamRoots}},
      gameWindows_{config::read().insideGamescope ? std::make_unique<session::GamescopeWindows>()
                                                  : nullptr},
      handoff_{config::read().executablePath, config::read().session, steam_, gameWindows_.get()},
      signIn_{StoreSignIn::Options{.dataDir = config::read().dataDir}} {
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

std::filesystem::path ShellApp::padsDirectory(bool hidden) {
    if (!hidden) {
        return "/dev/input";
    }
    const std::filesystem::path empty = config::read().dataDir / "hidden-pads";
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
    const input::TextInput typed = input::readTextInput();
    if (searching) {
        handleSearchText(typed);
        return;
    }
    if (typingPath) {
        handlePathText(typed);
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

void ShellApp::handlePathText(const input::TextInput& typed) {
    if (!typed.any()) {
        return;
    }
    shell_.inputDevice().noteKey();
    paths_.typeText(typed.text);
    if (typed.backspace) {
        paths_.backspace();
    }
    if (typed.enter) {
        paths_.confirm();
    } else if (typed.escape) {
        paths_.dismiss();
    }
}

void ShellApp::handlePads() {
    const std::vector<gamepad::Event> events = pads_.takeEvents();
    for (const gamepad::Event& event : events) {
        shell_.inputDevice().notePad(event);
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
    if (gameKeys_) {
        handleEvents(router_.fromGame(*gameKeys_));
    }
}

void ShellApp::actOn(gamepad::Button button) {
    if (shell_.inGame()) {
        actInGame(button);
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

void ShellApp::actInGame(gamepad::Button button) {
    ui::GameMenu& menu = shell_.gameMenu();
    if (button == gamepad::Button::Guide) {
        setGameMenuOpen(!menu.isOpen());
        return;
    }
    if (!menu.isOpen()) {
        return;
    }
    switch (button) {
    case gamepad::Button::Up:
        moveMenu(-1);
        break;
    case gamepad::Button::Down:
        moveMenu(1);
        break;
    case gamepad::Button::B:
        setGameMenuOpen(false);
        break;
    case gamepad::Button::A:
        if (menu.selected() == ui::GameMenuAction::CloseGame) {
            lucent::info("launch", "closing {} from the Guide menu", runningTitle_);
            handoff_.forceClose();
        }
        setGameMenuOpen(false);
        break;
    default:
        break;
    }
}

void ShellApp::moveMenu(int delta) {
    ui::GameMenu& menu = shell_.gameMenu();
    const ui::GameMenuAction before = menu.selected();
    menu.move(delta);
    // input-sound.md 3.4 Navigation: a focus move in a list.
    if (menu.selected() != before) {
        sounds_.play(audio::Effect::Navigation);
    }
}

void ShellApp::setGameMenuOpen(bool open) {
    ui::GameMenu& menu = shell_.gameMenu();
    // input-sound.md 3.4 OpenContextMenu / Close: a menu becomes visible or hidden (r90.java).
    sounds_.play(open ? audio::Effect::OpenContextMenu : audio::Effect::Close);
    if (open) {
        menu.open(runningTitle_);
    } else {
        menu.close();
    }
    if (padsHeld_) {
        pads_.setBlocked(open);
    }
    if (overlay_) {
        overlay_->setShown(open);
    } else if (!settings_.hidden) {
        if (open) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
        } else {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        }
    }
}

void ShellApp::chooseIconSize(int level) {
    if (shell_.modeChooser().isOpen()) {
        layoutPicker_.chooseIconSize(level);
    } else if (shell_.settingsPanels().onlyPage()) {
        settingsScreen_.chooseLevel(level);
    }
}

void ShellApp::contextMenu(const ui::PointerTarget& target) {
    if (shell_.inGame() || panels_.active()) {
        return;
    }
    // Right click dismisses an open menu from outside it, and opens a tile's menu; it is never
    // Back.
    if (shell_.contextMenu().isOpen()) {
        if (std::holds_alternative<ui::OnContextBackdrop>(target)) {
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
    return trailOf(state);
}

void ShellApp::activateCrumb(std::size_t index) {
    const ui::Trail trail = currentTrail();
    if (index >= trail.size() || !ui::isPlace(trail[index]) || shell_.inGame() ||
        panels_.active()) {
        return;
    }
    // The details page and the Settings screen are the only panels the trail stays clickable over.
    details_.close();
    if (shell_.panelOpen() && !shell_.settingsPanels().onlyPage()) {
        return;
    }
    const ui::Crumb& crumb = trail[index];
    if (crumb.kind == ui::CrumbKind::Settings) {
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
    if (paths_.active()) {
        return ui::HintContext{};
    }
    if (shortcutEditor_.capturing()) {
        return ui::HintContext{.back = true};
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
    // iiSU's Home has no title (pl3.q); opensu's names the focused tile everywhere, and the
    // trail names the game on its details page.
    shell_.setTitle(shell_.detailsPage().isOpen() ? std::string{} : shell_.pillTitle());
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
    shell_.setClock(ui::ClockText::format(parts.tm_hour, parts.tm_min, config::read().clock24Hour));
    nextClockTick_ = std::chrono::steady_clock::now() +
                     ui::ClockText::untilNextMinute(parts.tm_sec, millisecond);
    // STOPGAP: the battery is re-read on the clock's minute tick because opensu has no
    // power_supply uevent listener standing in for iiSU's ACTION_BATTERY_CHANGED receiver.
    shell_.setBattery(battery_.read());
}

ShellSnapshot ShellApp::snapshot() const {
    const std::lock_guard lock{stateMutex_};
    return published_;
}

void ShellApp::inject(gamepad::Button button, input::Device device) {
    const std::lock_guard lock{injectedMutex_};
    injected_.emplace_back(button, device);
}

void ShellApp::injectKey(input::Combo combo) {
    const std::lock_guard lock{injectedMutex_};
    injectedKeys_.push_back(combo);
}

void ShellApp::typeText(std::string text) {
    const std::lock_guard lock{injectedMutex_};
    typed_.push_back(std::move(text));
}

void ShellApp::requestClose() {
    closeRequested_.store(true);
}

void ShellApp::requestCatalogReload(std::string toast) {
    requestToast(std::move(toast), false);
    reloadRequested_.store(true);
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

bool ShellApp::captureFrame(std::string& png) {
    std::unique_lock lock{stateMutex_};
    if (capturePending_) {
        // One frame request at a time: two callers racing for the context would
        // interleave, and the second would get the first's answer.
        return false;
    }
    capturePending_ = true;
    captureResult_.clear();
    // The loop answers within a frame or two. The bound stops a caller hanging
    // forever if the loop has already exited.
    captureAnswered_.wait_for(lock, std::chrono::seconds{5}, [this] {
        return !capturePending_;
    });
    if (capturePending_) {
        // Timed out, so the request is abandoned rather than left set.
        capturePending_ = false;
        return false;
    }
    png = captureResult_;
    captureResult_.clear();
    return !png.empty();
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
    next.gameMenuOpen = shell_.gameMenu().isOpen();
    next.steam = std::string{launch::name(steam_.state())};
    next.inputDevice =
        shell_.inputDevice().current() == input::Device::KeyboardMouse ? "keyboard" : "pad";
    next.launchers = describe(launcherBadges(steam_.state(), steam_.downloads(), sources_));

    const std::lock_guard lock{stateMutex_};
    published_ = std::move(next);
}

void ShellApp::serviceControlRequests() {
    // Buttons injected over the channel take the same path as a real press, so
    // what the channel exercises is the shell's own handling.
    std::vector<std::pair<gamepad::Button, input::Device>> queued;
    std::vector<std::string> typed;
    std::vector<input::Combo> keys;
    {
        const std::lock_guard lock{injectedMutex_};
        queued.swap(injected_);
        typed.swap(typed_);
        keys.swap(injectedKeys_);
    }
    for (const input::Combo& combo : keys) {
        shell_.inputDevice().noteKey();
        if (shortcutEditor_.capturing()) {
            shortcutEditor_.captureKey(combo);
        } else {
            handleEvents(router_.fromCombo(combo));
        }
    }
    for (const std::string& text : typed) {
        if (shell_.searchPanel().isOpen()) {
            shell_.inputDevice().noteKey();
            search_.typeText(text);
        } else if (paths_.typing()) {
            shell_.inputDevice().noteKey();
            paths_.typeText(text);
        }
    }
    // An injected button is a tap: without its release a direction would repeat forever.
    for (const auto& [button, device] : queued) {
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

    bool wanted = false;
    {
        const std::lock_guard lock{stateMutex_};
        wanted = capturePending_;
    }
    if (!wanted) {
        return;
    }

    std::string png;
    const bool ok = renderShellPng(shell_, png);
    const std::lock_guard lock{stateMutex_};
    captureResult_ = ok ? std::move(png) : std::string{};
    capturePending_ = false;
    captureAnswered_.notify_all();
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
    if (reloadRequested_.exchange(false)) {
        reloadCatalog();
    }
    const bool running = gameRunning_.load();
    if (running != shell_.inGame()) {
        shell_.setInGame(running);
        shell_.gameMenu().close();
        panels_.endLaunch();
        // raylib has no ShowWindow or HideWindow: hiding is a window state flag, and showing is
        // clearing it.
        if (overlay_) {
            if (running) {
                overlay_->enter();
            } else {
                overlay_->leave();
            }
        } else if (!settings_.hidden) {
            if (running) {
                SetWindowState(FLAG_WINDOW_HIDDEN);
            } else {
                ClearWindowState(FLAG_WINDOW_HIDDEN);
            }
        }
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
    if (config::read().insideGamescope && !settings_.hidden) {
        // Gamescope composites a window as the overlay only when it spans the whole screen,
        // which also draws the home screen at the output's own resolution.
        const int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        shell_.setSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        overlay_ = std::make_unique<session::GamescopeOverlay>(
            *static_cast<const unsigned long*>(GetWindowHandle()));
        gameKeys_ = std::make_unique<session::GameKeys>();
    }
    sounds_.open();
    delivery_.loadStoredSounds();
    // Textures need a GL context; the shell streams artwork in as frames are drawn.
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(*this, signIn_, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background while the shell is already usable. A machine
    // without a Steam install has nothing to start, and shows no Steam icon.
    if (!library::steam::Library::discover(resolved_.home, resolved_.steamRoots).roots().empty()) {
        steam_.start();
    }

    while (!closeRequested_.load() && !WindowShouldClose()) {
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
    {
        const std::lock_guard lock{stateMutex_};
        capturePending_ = false;
        captureAnswered_.notify_all();
    }
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
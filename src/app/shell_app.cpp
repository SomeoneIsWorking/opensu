#include "shell_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <utility>

#include "artwork/iisu_assets.hpp"
#include "config/config.hpp"
#include "frame_png.hpp"
#include "game_end.hpp"
#include "gpu2d/gpu_error.hpp"
#include "input/key_names.hpp"
#include "input/keyboard_bindings.hpp"
#include "keyboard_pad.hpp"
#include "launcher_status.hpp"
#include "library/emulator_choice.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "retro_launch.hpp"
#include "ui/interface_input.hpp"
#include "ui/section_view.hpp"

namespace opensu::app {

ShellApp::ShellApp(const Settings& settings, const config::Config& environment)
    : environment_{environment}, settings_{settings},
      catalogLoader_{library::makeCatalog(resolved_, settings_.hidden, romsetLists_,
                                          integrations_.catalogStores())},
      shell_{settings_.width, settings_.height, settings_.homeMode, environment_.assetsDir},
      steam_{steam::Client::Options{
          .home = resolved_.home,
          .executablePath = resolved_.executablePath,
          .session = resolved_.session,
          .steamRoots = resolved_.steamRoots,
          .mayStart = settings_.startSteam,
          .sandbox = steam::SandboxPlaces{.runtimeDir = environment_.runtimeDir.parent_path(),
                                          .display = environment_.x11Display,
                                          .gamescopeWayland = environment_.gamescopeWaylandDisplay,
                                          .gpu = steam::gpuNodes("/dev")},
          .desktopBus = environment_.sessionBus,
          .secretBusSocket = environment_.runtimeDir / (resolved_.session + "-secret-bus")}},
      gameWindows_{environment_.insideGamescope ? std::make_unique<session::GamescopeWindows>()
                                                : nullptr},
      steamWindowHiding_{
          environment_.insideGamescope ? std::make_unique<session::SteamWindowHiding>() : nullptr},
      handoff_{environment_.executablePath, environment_.session, steam_, gameWindows_.get()},
      signIn_{StoreSignIn::Options{.dataDir = environment_.dataDir}} {
    shell_.shortcuts() = input::Shortcuts{preferences_.values().shortcuts};
    // input-sound.md 3.4: Navigation for a focus move or a press, Close for a page dismissed.
    shell_.screens().navigator().setSounds(
        gpu2d::ui::Navigator::Sounds{[this] {
                                         sounds_.play(audio::Effect::Navigation);
                                     },
                                     [this] {
                                         sounds_.play(audio::Effect::Navigation);
                                     },
                                     [this] {
                                         sounds_.play(audio::Effect::Close);
                                     }});
    shell_.screens().navigator().setKeyboard([this] {
        keyboardPad_.open();
    });
    padOwner_.syncChords();
    clock_.service(std::chrono::steady_clock::now());
    applyLayout();
    firstRun_.openIfUnchosen();
}

void ShellApp::applyIntegrations() {
    integrationSwitch_.apply();
    settingsScreen_.refresh();
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
    shell_.setUiScale(settings::uiScaleFactor(chosen.uiScale));
    shell_.setKeyboardStyle(chosen.keyboardStyle);
    sounds_.setMuted(!chosen.uiSounds);
}

std::vector<SearchResult> ShellApp::searchResults() const {
    std::vector<SearchResult> results;
    for (const ui::Tile& tile : shell_.tiles()) {
        const auto* game = std::get_if<library::Game>(&tile.item);
        results.push_back(SearchResult{game != nullptr ? game->title : tile.title,
                                       library::describeResult(tile.item)});
    }
    return results;
}

void ShellApp::openTile(std::size_t index) {
    shell_.focusTile(index);
    pressTile();
}

void ShellApp::pressTile() {
    shell_.pressFocused();
    if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
        openFolder(*folder);
    } else if (shell_.focusedGiveaways()) {
        sounds_.play(audio::Effect::EnterConsolesApps);
        giveaways_.open();
    } else {
        details_.open();
    }
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
    library::applyEmulatorPicks(gameSettings_.layers(), games_);
    plugins_.apply(games_);
    plugins_.request(games_);
    artworkStore_.apply(games_);
    covers_.catalogChanged(games_);
    const std::vector<library::Console> consoles = library::consoles(games_);
    artworkFetcher_.request(games_, consoles, artwork::allApkAssets());
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
    refreshShelf();
}

void ShellApp::pushCatalogToShell() {
    showShelf(shell_.focusIndex());
}

void ShellApp::showShelf(std::size_t focus) {
    const settings::Settings& chosen = preferences_.values();
    std::vector<library::ShelfItem> shelf =
        library::visibleShelf(browser_, games_, sources_, chosen.view, chosen.hidden);
    artworkStore_.apply(shelf);
    covers_.apply(shelf);
    shell_.setShelf(std::move(shelf), focus);
    finishShelf(chosen);
}

void ShellApp::refreshShelf() {
    const settings::Settings& chosen = preferences_.values();
    std::vector<library::ShelfItem> shelf =
        library::visibleShelf(browser_, games_, sources_, chosen.view, chosen.hidden);
    artworkStore_.apply(shelf);
    covers_.apply(shelf);
    shell_.updateShelf(std::move(shelf));
    finishShelf(chosen);
}

void ShellApp::finishShelf(const settings::Settings& chosen) {
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
        mailbox_.postToast(result.ok ? "sign in there; the opensu sign-in extension finishes it"
                                     : result.message,
                           !result.ok);
    }};
}

void ShellApp::handleEvents(const std::vector<gamepad::Event>& incoming) {
    if (shortcutEditor_.capturing() || mappings_.capturing()) {
        shortcutEditor_.capturePad(incoming);
        mappings_.capturePad(incoming);
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
            // The wheel's petals take the left stick, unless the Guide menu took the buttons.
            if (event.stick && keyboardPad_.takesStick() && !guideMenu_.isOpen()) {
                keyboardPad_.stick(directionOf(event.button), event.pressed);
            } else if (gamepad::DirectionRepeat::repeats(event.button)) {
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
    const bool typing = shell_.screens().navigator().typing();
    const input::KeyFrame& frame = window_->keys();
    if (typing) {
        typeIntoField(input::readTextInput(frame));
        return;
    }
    if (shortcutEditor_.capturing()) {
        if (frame.pressed(input::keys::Escape)) {
            shortcutEditor_.cancel();
        } else if (const std::optional<input::Combo> combo = input::pressedCombo(frame)) {
            shell_.inputDevice().noteKey();
            shortcutEditor_.captureKey(*combo);
        }
        return;
    }
    const ShortcutRouter::Keys keys = router_.fromKeys(frame);
    if (keys.any) {
        shell_.inputDevice().noteKey();
    }
    handleEvents(keys.events);
}

void ShellApp::typeIntoField(const input::TextInput& typed) {
    if (!typed.any()) {
        return;
    }
    shell_.inputDevice().noteKey();
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    navigator.text(typed.text);
    for (const ui::TypingKey& key : ui::typingKeys) {
        if (typed.*key.pressed) {
            navigator.act(key.action);
        }
    }
}

void ShellApp::handlePads() {
    const std::vector<gamepad::Event> events = padOwner_.admit(pads_->takeEvents());
    for (const gamepad::Event& event : events) {
        shell_.inputDevice().notePad(event);
        if (roster_.note(event)) {
            devicesScreen_.controllerChanged();
        }
    }
    handleEvents(events);
}

void ShellApp::handlePointer() {
    pointer_.route(window_->pointer());
}

void ShellApp::screens(const PointerFrame& frame) {
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    if (!frame.point) {
        return;
    }
    const gpu2d::Point at{frame.point->x, frame.point->y};
    if (frame.delta.x != 0.0f || frame.delta.y != 0.0f) {
        navigator.pointer(gpu2d::ui::PointerEvent{gpu2d::ui::PointerKind::Move, at, 0.0f});
    }
    if (frame.left) {
        navigator.pointer(gpu2d::ui::PointerEvent{gpu2d::ui::PointerKind::Press, at, 0.0f});
    }
    // A right click is the second action (X) of what it is over.
    if (frame.right) {
        navigator.pointer(gpu2d::ui::PointerEvent{gpu2d::ui::PointerKind::Move, at, 0.0f});
        navigator.act(gpu2d::ui::Action::Secondary);
    }
    if (frame.wheel != 0.0f) {
        navigator.pointer(gpu2d::ui::PointerEvent{gpu2d::ui::PointerKind::Wheel, at, frame.wheel});
    }
}

ui::PointerTarget ShellApp::pointAt(std::optional<ui::Point> point) {
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
    if (shell_.inGame() || shell_.panelOpen()) {
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
    const bool modal = shell_.inGame() || shell_.panelOpen();
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
    if (shell_.inGame() || shell_.panelOpen()) {
        return;
    }
    const int steps = library::Sections::stepsBetween(browser_.section(), section);
    if (steps != 0) {
        cycleSection(steps);
    }
}

void ShellApp::handleGameKeys() {
    if (GameOverlay* overlay = gameScreen_.overlay()) {
        // The watcher is for a game that has the keyboard; otherwise the shell's window keys are
        // the ones to act on, and acting on both would fire every shortcut twice.
        const bool gameHasKeyboard =
            shell_.inGame() && gameScreen_.mode() != session::OverlayMode::Interactive;
        const bool backOpensMenu =
            gameHasKeyboard && builtInOf(launches_.game(), emulatorSettings_.catalog()) != nullptr;
        handleEvents(router_.fromGame(*overlay, gameHasKeyboard, backOpensMenu));
    }
}

void ShellApp::actOn(gamepad::Button button) {
    if (button == gamepad::Button::Guide && focusFollow_.guideReturns(shell_.inGame())) {
        gameScreen_.takeFocus();
        return;
    }
    if (button == gamepad::Button::Guide) {
        toggleGuide(GuideMenuController::Landing::Places);
        return;
    }
    if (keyboardPad_.isOpen()) {
        keyboardPad_.act(button);
        return;
    }
    // Over a running game only the pages the Guide menu opened take buttons.
    if (shell_.inGame() && !shell_.panelOpen()) {
        return;
    }
    if (mappings_.active()) {
        mappings_.act(button);
        return;
    }
    if (shell_.screens().active()) {
        if (const std::optional<gpu2d::ui::Action> action =
                ui::interfaceActionOf(button, shell_.screens().navigator().typing());
            action && !shortcutEditor_.capturing()) {
            shell_.screens().navigator().act(*action);
        }
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
        pressTile();
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
        } else if (shell_.inputDevice().current() == input::Device::KeyboardMouse) {
            // ESC at the root of the shell has nothing to go back to; it opens the Guide menu.
            toggleGuide(GuideMenuController::Landing::Places);
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
    library::Game started = game;
    const library::wine::Prepared wine = wineSettings_.prepare(started);
    if (!wine.refusal.empty()) {
        shell_.setToast(wine.refusal, true);
        return;
    }
    if (launches_.running()) {
        shell_.setToast("a game is already running", true);
        return;
    }
    std::string control = retro_.begin();
    if (const std::string refused =
            prepareRetroLaunch(started, gameSettings_.layers(), emulatorSettings_.catalog(),
                               retro_.settingsFile(), emulatorDownloads_.shaderPack().root());
        !refused.empty()) {
        static_cast<void>(retro_.end());
        shell_.setToast("cannot write " + game.title + "'s settings: " + refused, true);
        return;
    }

    // input-sound.md 3.4 OpenAppRom: every game launch.
    sounds_.play(audio::Effect::OpenAppRom);
    panels_.showLaunch(game.title);
    recordLaunch(game);

    std::vector<std::string> environment = padOwner_.hold(mappings_.forGame(game));
    environment.push_back(std::move(control));
    environment.insert(environment.end(), wine.environment.begin(), wine.environment.end());
    launches_.start(std::move(started), std::move(environment));
}

void ShellApp::offerInstall(const library::Game& game) {
    // A store's own page installs that store's copy; elsewhere any store that owns the title will
    // do.
    panels_.offerInstall(game, browser_.inLauncher() ? std::vector<library::Game>{game}
                                                     : library::copiesOf(games_, game));
}

void ShellApp::cancelLaunch() {
    launches_.cancel();
}

void ShellApp::openGameSettings(const library::Game& game, EditorLook look) {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    navigator.push(std::make_unique<LayeredEditor>(layered_, navigator, shell_.screens().chrome(),
                                                   layered_.forGame(game), look, game.title));
}

const library::Game* ShellApp::gameInFront() const {
    if (!shell_.inGame()) {
        return nullptr;
    }
    if (focusFollow_.following() && focusFollow_.holder() != session::FocusHolder::Game) {
        return nullptr;
    }
    return &launches_.game();
}

bool ShellApp::menusBlocked() {
    return panels_.active() || paths_.active() || artworkSettings_.typing() || mappings_.active() ||
           sessionInstall_.active() || firstRun_.active() || shortcutEditor_.capturing() ||
           search_.isOpen() || layoutPicker_.isOpen();
}

void ShellApp::closeRunningGame() {
    launches_.close();
}

void ShellApp::toggleGuide(GuideMenuController::Landing landing) {
    if (guideMenu_.isOpen() || !menusBlocked()) {
        guideMenu_.toggle(landing);
    }
}

void ShellApp::closePages() {
    mappings_.close();
    details_.close();
    settingsScreen_.close();
    giveaways_.close();
    devicesScreen_.close();
    downloadsScreen_.close();
    // A game's settings, opened over its details or the running game.
    shell_.screens().navigator().clear();
}

void ShellApp::openDownloads() {
    closePages();
    downloadsScreen_.open();
}

void ShellApp::syncOverlay() {
    using session::OverlayMode;
    shell_.followChordHint(router_.longHeld(std::chrono::steady_clock::now()));
    const OverlayMode over = shell_.panelOpen()          ? OverlayMode::Interactive
                             : shell_.chordHintVisible() ? OverlayMode::Passive
                                                         : OverlayMode::Hidden;
    gameScreen_.show(shell_.inGame() ? over : OverlayMode::Hidden);
}

std::string ShellApp::outputLine() const {
    const std::optional<session::Output> output = window_ ? window_->output() : std::nullopt;
    if (!output) {
        return {};
    }
    return std::to_string(output->width) + " x " + std::to_string(output->height) + " at " +
           std::to_string(output->refreshHz) + " Hz";
}

void ShellApp::serviceDevices(std::chrono::steady_clock::time_point now) {
    if (bluetooth_.poll(now, devicesScreen_.showsBluetooth())) {
        devicesScreen_.refresh();
    }
    if (const std::optional<BluetoothNotice> notice = bluetooth_.takeNotice()) {
        shell_.setToast(notice->text, notice->error);
    }
    devicesScreen_.tick(now);
}

void ShellApp::contextMenu(const ui::PointerTarget& target) {
    if (shell_.inGame() || sessionInstall_.active()) {
        return;
    }
    // Right click opens a tile's menu; it is never Back.
    if (shell_.panelOpen() || !std::holds_alternative<ui::OnTile>(target)) {
        return;
    }
    focus(target);
    contextMenu_.open();
}

void ShellApp::setHidden(const library::Game& game, bool hidden) {
    preferences_.values().hidden.set(game, hidden);
    preferences_.save();
    refreshShelf();
    shell_.setToast(game.title + (hidden ? " hidden" : " shown again"));
}

void ShellApp::recordLaunch(const library::Game& game) {
    preferences_.values().lastPlayed.record(game.id, std::chrono::system_clock::now());
    preferences_.values().lastPlayed.apply(games_);
    preferences_.save();
}

void ShellApp::chooseEmulator(const library::Game& game, const std::string& name) {
    const settings::Layer own{settings::Layer::Kind::Game, game.id};
    if (name.empty()) {
        gameSettings_.layers().clear(own, library::emulatorKey);
        gameSettings_.save();
        // The search's choice is found again by a fresh read.
        reloadCatalog();
        return;
    }
    gameSettings_.layers().set(own, library::emulatorKey, name);
    gameSettings_.save();
    library::applyEmulatorPicks(gameSettings_.layers(), games_);
    refreshShelf();
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

const library::Game* ShellApp::ownGame(const std::string& id) const {
    const auto found = std::ranges::find(games_, id, &library::Game::id);
    return found == games_.end() ? nullptr : &*found;
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
    if (const ui::GameDetails* details = details_.details()) {
        state.game = details->title;
    }
    state.settings = settingsScreen_.category();
    state.giveaways = giveaways_.isOpen();
    state.offer = giveaways_.openOffer();
    state.devices = devicesScreen_.tab();
    state.downloads = downloadsScreen_.isOpen();
    return trailOf(state);
}

void ShellApp::activateCrumb(std::size_t index) {
    const ui::Trail trail = currentTrail();
    if (index >= trail.size() || !ui::isPlace(trail[index]) || shell_.inGame() ||
        panels_.active() || firstRun_.active()) {
        return;
    }
    // The details page, the Settings screen and the Devices page are the only panels the trail
    // stays clickable over.
    details_.close();
    const bool settingsAlone =
        settingsScreen_.isOpen() && !paths_.active() && !shell_.keyboard().isOpen();
    if (shell_.panelOpen() && !settingsAlone && !devicesScreen_.isOpen()) {
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
    if (mappings_.active() || shell_.keyboard().isOpen()) {
        return ui::HintContext{};
    }
    // The toolkit screens' hint bar names what their focus does.
    if (shell_.screens().active() || giveaways_.isOpen()) {
        return ui::HintContext{};
    }
    if (shortcutEditor_.capturing()) {
        return ui::HintContext{.back = true};
    }
    const bool game = shell_.focusedGame() != nullptr;
    const bool folder = shell_.focusedFolder().has_value();
    return ui::HintContext{.back = browser_.canBack(),
                           .clearSearch = search_.searching(),
                           .select = game || folder || shell_.focusedGiveaways(),
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

void ShellApp::publishSnapshot() {
    ShellSnapshot next;
    next.games = games_.size();
    next.installed = installedCount_;
    next.artworkPending = shell_.artworkPending();
    if (const library::Game* focused = shell_.focusedGame(); focused != nullptr) {
        next.focusedId = focused->id;
    } else if (const std::optional<library::Folder> folder = shell_.focusedFolder()) {
        next.focusedId = std::holds_alternative<library::Console>(*folder)
                             ? "console:" + library::key(*folder)
                             : library::key(*folder);
    } else if (shell_.focusedGiveaways()) {
        next.focusedId = "giveaways";
    }
    next.focusedTitle = shell_.focusedTitle();
    next.section = std::string{library::key(browser_.section())};
    next.libraryMode = std::string{library::key(preferences_.values().libraryMode)};
    next.modeChooserOpen = layoutPicker_.isOpen();
    next.searchOpen = search_.isOpen();
    next.searchText = preferences_.values().view.search;
    next.contextMenuOpen = contextMenu_.isOpen();
    next.detailsOpen = details_.isOpen();
    next.settingsOpen = settingsScreen_.isOpen();
    if (const gpu2d::ui::Screen* top = shell_.screens().navigator().top()) {
        next.screen = top->id();
        next.screenFocus = shell_.screens().navigator().focused();
    }
    next.firstRunOpen = firstRun_.active();
    next.folderPickerOpen = paths_.active();
    next.capturingShortcut = shortcutEditor_.capturing();
    next.mapping = shell_.mappingPage().state();
    next.mappingFocus = shell_.mappingPage().focusName();
    next.volumeShown = shell_.volumeOsd().holding();
    next.chordHint = gamepad::name(shell_.chordHint().modifier().value_or(gamepad::Button::None));
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
    next.launching = launches_.running();
    // What the loop has applied, not what was asked for, so a request that never reached the
    // loop cannot read as in game.
    next.inGame = shell_.inGame();
    next.guideMenuOpen = guideMenu_.isOpen();
    for (const gpu2d::ui::Screen* screen : shell_.screens().navigator().screens()) {
        if (const auto* editor = dynamic_cast<const LayeredEditor*>(screen)) {
            next.settingsScope = editor->scopeName();
        }
    }
    next.devicesOpen = devicesScreen_.isOpen();
    next.devicesTab = devicesScreen_.tab();
    next.downloadsOpen = downloadsScreen_.isOpen();
    next.downloads = downloadsScreen_.lines();
    next.steam = std::string{launch::name(steam_.state())};
    next.inputDevice =
        shell_.inputDevice().current() == input::Device::KeyboardMouse ? "keyboard" : "pad";
    next.launchers = describe(launcherBadges(steam_.state(), steam_.downloads(), sources_));

    bridge_.publish(std::move(next));
}

void ShellApp::serviceControlRequests() {
    // Buttons injected over the channel take the same path as a real press, so
    // what the channel exercises is the shell's own handling.
    const ControlBridge::Input queued = bridge_.takeInput(std::chrono::steady_clock::now());
    for (const input::Combo& combo : queued.keys) {
        shell_.inputDevice().noteKey();
        if (shortcutEditor_.capturing()) {
            shortcutEditor_.captureKey(combo);
        } else {
            handleEvents(router_.fromCombo(combo));
        }
    }
    for (const std::string& text : queued.text) {
        if (shell_.screens().navigator().typing()) {
            shell_.inputDevice().noteKey();
            shell_.screens().navigator().text(text);
        }
    }
    for (const PointerInjection& injected : queued.pointer) {
        PointerFrame frame;
        frame.point = ui::Point{injected.x, injected.y};
        // A move always moves, so it hovers even onto the spot the pointer was last put.
        if (injected.kind == PointerInjection::Kind::Move) {
            frame.delta = ui::Point{1.0f, 1.0f};
        }
        frame.left = injected.kind == PointerInjection::Kind::Click;
        frame.wheel = injected.wheel;
        pointer_.route(frame);
    }
    // An injection carries its releases: without them a direction would repeat forever.
    for (const auto& [event, device] : queued.buttons) {
        if (device == input::Device::KeyboardMouse) {
            shell_.inputDevice().noteKey();
        } else {
            shell_.inputDevice().notePad(event);
        }
        handleEvents({event});
    }

    if (!bridge_.frameWanted()) {
        return;
    }
    std::string png;
    bridge_.answerFrame(renderShellPng(*window_, shell_, png) ? std::move(png) : std::string{});
}

/// Applies what the launch thread and the control channel asked for. Everything
/// that touches the window or the shell happens here, on the loop's thread.
void ShellApp::serviceRequests() {
    const bool launching = launches_.running();
    if (padOwner_.held() && !launching) {
        padOwner_.release();
    }
    if (const std::optional<launch::LaunchProgress> progress = mailbox_.takeProgress()) {
        const bool measured = progress->stage == launch::LaunchProgress::Stage::Updating;
        panels_.update(launch::describe(*progress),
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
    const bool running = mailbox_.gameRunning();
    if (running != shell_.inGame()) {
        shell_.setInGame(running);
        closePages();
        panels_.endLaunch();
        gameScreen_.setRunning(running);
        if (!running) {
            // Emulators write their play time as a game ends.
            plugins_.request(games_);
        }
    }
    if (std::optional<LaunchMailbox::Toast> toast = mailbox_.takeToast()) {
        shell_.setToast(std::move(toast->text), toast->error);
    }
}

int ShellApp::run() {
    reloadCatalog();

    if (!openWindow("openSU", settings_.hidden)) {
        return 1;
    }
    if (environment_.insideGamescope && !settings_.hidden) {
        // Gamescope composites a window as the overlay only when it spans the whole screen,
        // which also draws the home screen at the output's own resolution.
        const gpu2d::PixelSize size = window_->fillMonitor();
        shell_.setSize(size.width, size.height);
        gameScreen_.attachOverlay();
        focusFollow_.attach(std::make_unique<session::GamescopeWindows>(), window_->x11Window());
    }
    // A hidden run makes no sound.
    if (!settings_.hidden) {
        sounds_.open();
    }
    delivery_.loadStoredSounds();

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(bridge_, signIn_, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background, in its mode, while the shell is already usable.
    integrationSwitch_.begin();

    while (!bridge_.closeRequested() && window_->pollEvents()) {
        // A resize changes the framebuffer, so the layout is recomputed before anything is drawn.
        if (const std::optional<gpu2d::PixelSize> size = window_->takeResize()) {
            shell_.setSize(size->width, size->height);
        }

        integrationSwitch_.service();
        padOwner_.lendToSteam(steamGridDbAccount_.signingIn() || offerClaim_.inSteam());
        focusFollow_.service(padOwner_.held());
        frameGeneration_.follow(gameInFront());
        padOwner_.follow(focusFollow_.following(), focusFollow_.holder(),
                         gameScreen_.mode() == session::OverlayMode::Interactive);
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
        plugins_.service();
        delivery_.service();
        covers_.service();
        serviceDevices(std::chrono::steady_clock::now());
        sessionInstall_.service();
        emulatorDownloads_.service();
        wineRunners_.service(std::chrono::steady_clock::now());
        downloadsScreen_.service();
        settingsScreen_.service();
        giveaways_.service();
        keyboardPad_.follow();
        syncOverlay();
        syncChrome();
        shell_.tick(std::chrono::steady_clock::now());
        publishSnapshot();
        // An unmapped window draws nothing, and its artwork still streams in.
        shell_.loadArtwork();
        window_->present([this](ui::Canvas& canvas, const gpu2d::Surface& target) {
            shell_.draw(canvas, target);
        });
        clock_.service(std::chrono::steady_clock::now());
    }

    // Anything waiting on a frame will never get one now.
    bridge_.abandonFrame();
    if (control_) {
        control_->stop();
    }
    return 0;
}

bool ShellApp::openWindow(const std::string& title, bool hidden) {
    try {
        window_ = std::make_unique<ShellWindow>(
            ShellWindow::Options{title, settings_.width, settings_.height, hidden});
        shell_.attach(window_->device());
    } catch (const gpu2d::GpuError& error) {
        lucent::error("shell", "could not open a window: {}", error.what());
        window_.reset();
        return false;
    }
    gameScreen_.attach(*window_);
    return true;
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

    // Textures need a device, and the device a window, which is never mapped.
    if (!openWindow("opensu render", true)) {
        return false;
    }
    shell_.loadArtworkNow();
    lucent::info("render", "loaded artwork for {} of {} tiles", shell_.loadedArtwork(),
                 shell_.tiles().size());
    // A still frame shows the grid at rest, after its entrance.
    shell_.tick(std::chrono::steady_clock::now());
    syncChrome();
    shell_.settle();

    std::string png;
    const bool ok = renderShellPng(*window_, shell_, png);
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
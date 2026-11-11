#include "shell_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

#include <SDL3/SDL_mouse.h>

#include "config/config.hpp"
#include "game_end.hpp"
#include "game_overlay.hpp"
#include "gpu2d/gpu_error.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "input/chord_set.hpp"
#include "input/key_names.hpp"
#include "input/keyboard_bindings.hpp"
#include "keyboard_pad.hpp"
#include "launcher_status.hpp"
#include "library/emulator_choice.hpp"
#include "library/linked_play.hpp"
#include "library/match_pick.hpp"
#include "library/titles.hpp"
#include "lucent/log.h"
#include "retro/save_paths.hpp"
#include "retro_launch.hpp"
#include "screen_orientation.hpp"
#include "shell_frame.hpp"
#include "stream/remote_reach.hpp"
#include "ui/interface_input.hpp"
#include "ui/section_view.hpp"

namespace opensu::app {
namespace {

/// A launch held for its saves, shared by the held call so copying it cannot throw.
struct HeldLaunch {
    library::Game game;
    std::vector<std::string> environment;
    std::string profile;
};

} // namespace

ShellApp::ShellApp(const Settings& settings, const config::Config& environment)
    : environment_{environment}, settings_{settings},
      catalogLoader_{catalogOf(integrations_.catalogStores())},
      shell_{settings_.width, settings_.height,
             settings_.homeMode.value_or(config::HomeMode::Standard), environment_.assetsDir},
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
          .secretBusSocket = environment_.runtimeDir / (resolved_.session + "-secret-bus"),
          .containment = containment_}},
      session_{openShellSession(environment_)},
      handoff_{environment_.executablePath, environment_.session, steam_,
               session_ ? session_->gameWindows() : nullptr, environment_.gameOutputDir,
               containment_},
      signIn_{StoreSignIn::Options{.dataDir = environment_.dataDir}} {
    shell_.shortcuts() = input::Shortcuts{preferences_.values().shortcuts};
    saveSharing_.share(preferences_.values());
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
    shell_.screens().navigator().setClipboard([this] {
        return window_->clipboard();
    });
    retro_.closeLeftovers();
    buildHome();
    padOwner_.syncChords();
    clock_.service(std::chrono::steady_clock::now());
    applyLayout();
    if (!profiles_.moveProblem().empty()) {
        shell_.setToast("Some saves from before profiles stay where they were; see the log", true);
    }
    // A tool's hidden run plays as the active profile without asking.
    if (!settings_.hidden) {
        askProfile();
    }
}

void ShellApp::switchProfile(const std::string& id) {
    const std::optional<profiles::Profile> next = profiles_.book().find(id);
    if (!next || next->id == profiles_.book().active().id) {
        return;
    }
    if (launches_.running() || remotePlay_.playing()) {
        shell_.setToast("Close the game first to play as " + next->name, true);
        return;
    }
    std::string error;
    if (!profiles_.book().activate(id, error)) {
        shell_.setToast("Cannot play as " + next->name + ": " + error, true);
        return;
    }
    lucent::info("profiles", "playing as {}", next->id);
    shell_.setToast("Playing as " + next->name, false);
    settingsScreen_.refresh();
    details_.refresh();
}

void ShellApp::askProfile() {
    const std::vector<profiles::Profile> all = profiles_.book().all();
    if (all.size() < 2) {
        return;
    }
    const std::string active = profiles_.book().active().id;
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    std::size_t focus = 0;
    for (const profiles::Profile& profile : all) {
        if (profile.id == active) {
            focus = choices.size();
        }
        choices.push_back(gpu2d::ui::Dialog::Choice{.label = profile.name,
                                                    .chosen =
                                                        [this, id = profile.id] {
                                                            switchProfile(id);
                                                        },
                                                    .kind = gpu2d::ui::ButtonKind::Normal,
                                                    .exits = profile.id == active});
    }
    shell_.screens().navigator().push(std::make_unique<gpu2d::ui::Dialog>(
        "Who is playing?", "Games load and keep the saves of the profile picked.",
        std::move(choices), focus));
}

void ShellApp::buildHome() {
    shell_.screens().navigator().setRoot(std::make_unique<HomeScreen>(
        shell_.grid(), HomeScreen::Hooks{.inGame =
                                             [this] {
                                                 return shell_.inGame();
                                             },
                                         .press =
                                             [this] {
                                                 pressTile();
                                             },
                                         .details =
                                             [this] {
                                                 details_.open();
                                             },
                                         .refresh =
                                             [this] {
                                                 reloadCatalog();
                                                 shell_.setToast("library refreshed");
                                             },
                                         // iiSU `pb0.java:2086`: SELECT asks for the item's menu.
                                         .options =
                                             [this] {
                                                 contextMenu_.open();
                                             },
                                         .search =
                                             [this] {
                                                 search_.open();
                                             },
                                         .menu =
                                             [this] {
                                                 toggleGuide(GuideMenuController::Landing::Places);
                                             },
                                         .back =
                                             [this] {
                                                 backOnHome();
                                             }}));
    shell_.topBar().setHooks(ui::TopBarChrome::Hooks{.crumb =
                                                         [this](std::size_t index) {
                                                             activateCrumb(index);
                                                         },
                                                     .launcher =
                                                         [this](const ui::LauncherBadge& badge) {
                                                             // A badge that shows a download opens
                                                             // the Downloads page.
                                                             if (badge.progress) {
                                                                 openDownloads();
                                                             } else {
                                                                 selectLauncher(badge.source);
                                                             }
                                                         },
                                                     .button =
                                                         [this](ui::TouchButton button) {
                                                             // Back and the menu as B and Guide.
                                                             actOn(button == ui::TouchButton::Back
                                                                       ? gamepad::Button::B
                                                                       : gamepad::Button::Guide);
                                                         }});
    shell_.cornerHints().onPress([this](gamepad::Button button) {
        actOn(button);
    });
    shell_.dock().onChoose([this](library::Section section) {
        clickSection(section);
    });
    shell_.keyboardChrome().onPress([this] {
        keyboardPad_.act(gamepad::Button::A);
    });
}

void ShellApp::applyIntegrations() {
    integrationSwitch_.apply();
    settingsScreen_.refresh();
}

void ShellApp::applyLayout() {
    const settings::Settings& chosen = preferences_.values();
    ui::HomeGrid& grid = shell_.grid();
    if (grid.libraryMode() != chosen.libraryMode) {
        grid.setLibraryMode(chosen.libraryMode);
    }
    shell_.dock().setPinLibrary(chosen.pinLibraryDock);
    if (grid.iconSize() != chosen.iconSize) {
        grid.setIconSize(chosen.iconSize);
    }
    shell_.setTheme(interfaceThemeSettings_.current());
    grid.setHomeMode(interfaceThemeSettings_.homeMode(settings_.homeMode));
    shell_.setUiScale(settings::uiScaleFactor(chosen.uiScale));
    shell_.setKeyboardStyle(chosen.keyboardStyle);
    if (standing_ != chosen.portrait) {
        standing_ = chosen.portrait;
        screen_orientation::stand(chosen.portrait);
    }
    sounds_.setMuted(!chosen.uiSounds);
    interfaceSounds_.apply(chosen, interfaceThemeSettings_.current());
}

std::vector<SearchResult> ShellApp::searchResults() const {
    std::vector<SearchResult> results;
    for (const ui::Tile& tile : shell_.grid().tiles()) {
        const auto* game = std::get_if<library::Game>(&tile.item);
        results.push_back(SearchResult{game != nullptr ? game->title : tile.title,
                                       library::describeResult(tile.item)});
    }
    return results;
}

void ShellApp::openTile(std::size_t index) {
    shell_.grid().focusTile(index);
    pressTile();
}

void ShellApp::pressTile() {
    ui::HomeGrid& grid = shell_.grid();
    grid.pressFocused();
    if (const std::optional<library::Folder> folder = grid.focusedFolder()) {
        openFolder(*folder);
    } else if (grid.focusedGiveaways()) {
        sounds_.play(audio::Effect::EnterConsolesApps);
        giveaways_.open();
    } else if (const library::AddTile* add = grid.focusedAdd()) {
        libraryAdditions_.press(add->id);
    } else {
        details_.open();
    }
}

void ShellApp::reloadCatalog() {
    catalogLoader_.refresh();
}

void ShellApp::addRomFolder(const std::filesystem::path& folder) {
    preferences_.values().addRomFolder(folder);
    preferences_.save();
    libraryFoldersChanged();
}

void ShellApp::libraryFoldersChanged() {
    const config::Config resolved = settings::resolved(environment_, preferences_.values());
    resolved_.romRoots = resolved.romRoots;
    resolved_.findRomRoots = resolved.findRomRoots;
    resolved_.gameFolders = resolved.gameFolders;
    catalogLoader_.replace(catalogOf(integrations_.catalogStores()));
}

library::Catalog ShellApp::catalogOf(const library::CatalogStores& stores) {
    library::Catalog catalog = library::makeCatalog(resolved_, settings_.hidden,
                                                    files_.ownGamesFile(), romsetLists_, stores);
    if (stores.lan && stream::playingMissing().empty()) {
        catalog.add(lan::remoteProvider(remoteGames_));
    }
    return catalog;
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
    library::markCopies(games_);
    sources_ = std::move(snapshot.sources);
    preferences_.values().lastPlayed.apply(games_);
    library::shareTitleData(games_,
                            {gameSettings_.keptData(), mappings_.keptData(), covers_.keptData()});
    library::applyEmulatorPicks(gameSettings_.layers(), games_);
    library::applyMatchPicks(gameSettings_.layers(), games_);
    plugins_.apply(games_);
    plugins_.request(games_);
    artworkStore_.apply(games_);
    covers_.catalogChanged(games_);
    // With the covers already on disk; one fetched later is shared from the next listing.
    shareLibrary();
    lanRoms_.share(games_);
    saveSharing_.know(games_, emulatorSettings_.catalog());
    requestArtwork();
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

void ShellApp::requestArtwork() {
    const settings::Settings& chosen = preferences_.values();
    std::shared_ptr<const artwork::IisuPack> iisu = consoleThemes_.iisu().pack();
    if (consoleTheme_ == nullptr || chosen.consoleTheme != consoleThemeId_ ||
        iisu != consoleThemeIisu_) {
        std::string error;
        consoleTheme_ = consoleThemes_.open(chosen.consoleTheme, error);
        if (consoleTheme_ == nullptr) {
            lucent::warn("artwork", "console art theme {}: {}; showing the default theme",
                         chosen.consoleTheme, error);
            consoleTheme_ = consoleThemes_.open({}, error);
        }
        consoleThemeId_ = chosen.consoleTheme;
        consoleThemeIisu_ = std::move(iisu);
    }
    std::vector<std::string> systems;
    for (const library::Console& console : library::consoles(games_)) {
        systems.push_back(console.system);
    }
    shell_.grid().consoleArt().use(consoleTheme_, systems);
    // The iiSU file placed serves the sounds too.
    interfaceSounds_.apply(chosen, interfaceThemeSettings_.current());
    artworkFetcher_.request(games_);
    delivery_.restart();
}

void ShellApp::pushCatalogToShell() {
    showShelf(shell_.grid().focusIndex());
}

void ShellApp::showShelf(std::size_t focus) {
    const settings::Settings& chosen = preferences_.values();
    std::vector<library::ShelfItem> shelf =
        library::visibleShelf(browser_, games_, sources_, libraryAdditions_.tiles(), chosen.view,
                              chosen.hidden, chosen.home);
    artworkStore_.apply(shelf);
    covers_.apply(shelf);
    shell_.grid().setShelf(std::move(shelf), browser_.takeReturn().value_or(focus));
    finishShelf(chosen);
}

void ShellApp::refreshShelf() {
    const settings::Settings& chosen = preferences_.values();
    std::vector<library::ShelfItem> shelf =
        library::visibleShelf(browser_, games_, sources_, libraryAdditions_.tiles(), chosen.view,
                              chosen.hidden, chosen.home);
    artworkStore_.apply(shelf);
    covers_.apply(shelf);
    if (const std::optional<std::size_t> slot = browser_.takeReturn()) {
        shell_.grid().setShelf(std::move(shelf), *slot);
    } else {
        shell_.grid().updateShelf(std::move(shelf));
    }
    finishShelf(chosen);
}

void ShellApp::finishShelf(const settings::Settings& chosen) {
    shell_.grid().setHeader(folderCard());
    shell_.setStatus(std::to_string(games_.size()) + " games · " + std::to_string(installedCount_) +
                     " installed" + (library::narrowing(chosen.view) ? " · filtered" : ""));
    std::vector<std::string> downloading = artworkFetcher_.pendingGames();
    std::ranges::copy(covers_.pendingGames(), std::back_inserter(downloading));
    shell_.grid().setArtworkDownloading(downloading);
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
    const std::size_t from = search_.searching() ? 0 : shell_.grid().focusIndex();
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
    const bool remote = remoteHasControls();
    for (const gamepad::Event& event : events) {
        if (remote && event.kind == gamepad::Event::Kind::Button &&
            event.button != gamepad::Button::Guide) {
            remoteInput_.button(event);
            continue;
        }
        // The relay has already given them to the game.
        if (relay_.forwarding() && event.kind == gamepad::Event::Kind::Button &&
            event.button != gamepad::Button::Guide) {
            continue;
        }
        // A remote pointing over a game in front points at the game, not the shell behind it.
        if (!gameInFront() && (wiiRemoteSettings_.take(event) || pointingPads_.take(event))) {
            continue;
        }
        switch (event.kind) {
        case gamepad::Event::Kind::Pointer:
        case gamepad::Event::Kind::PointerButton:
            break;
        case gamepad::Event::Kind::Connected:
            lucent::info("gamepad", "controller ready: {}", event.device);
            break;
        case gamepad::Event::Kind::Disconnected:
            shell_.setToast("controller disconnected", true);
            break;
        case gamepad::Event::Kind::Button:
            // The wheel's petals take the left stick, unless the Guide menu took the buttons.
            if (event.stick && keyboardPad_.takesStick() && !guideMenu_.inFront()) {
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
    if (input::pastePressed(frame) && shell_.screens().navigator().paste()) {
        shell_.inputDevice().noteKey();
        return;
    }
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
    if (remoteHasControls()) {
        // The keys themselves go to the remote game; only Guide stays here.
        std::vector<gamepad::Event> guide;
        std::ranges::copy_if(keys.events, std::back_inserter(guide), [](const gamepad::Event& e) {
            return e.button == gamepad::Button::Guide;
        });
        handleEvents(guide);
        return;
    }
    handleEvents(keys.events);
}

void ShellApp::typeIntoField(const input::TextInput& typed) {
    if (!typed.any()) {
        return;
    }
    // The system's on-screen keyboard is the finger's: it leaves the touch controls up.
    if (!keyboardPad_.systemTyping()) {
        shell_.inputDevice().noteKey();
    }
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
    // The phone's own navigation, not a keyboard: it leaves the touch controls up.
    if (window_->systemBack()) {
        // A remote game has no use for it, as a game on the runner has none: it opens the menu.
        actOn(remoteHasControls() || relay_.forwarding() ? gamepad::Button::Guide
                                                         : gamepad::Button::B);
    }
    if (!relay_.forwarding()) {
        pointer(window_->pointer());
    }
    const bool controls =
        remoteHasControls() && window_->hasTouchscreen() && pads_->connected().empty();
    remoteTouch_->panel(remotePlay_.panel());
    const display::Placed placed = remotePlacement();
    const touch::Box picture{.x = static_cast<float>(placed.x),
                             .y = static_cast<float>(placed.y),
                             .width = static_cast<float>(placed.width),
                             .height = static_cast<float>(placed.height)};
    handleEvents(
        remoteTouch_->follow(controls, window_->touchSurface(), picture, window_->fingers()));
}

void ShellApp::pointer(const PointerFrame& frame) {
    using gpu2d::ui::PointerEvent;
    using gpu2d::ui::PointerKind;
    if (remoteHasControls()) {
        if (!frame.touch || !remoteTouch_->shown()) {
            remoteInput_.pointer(frame, remotePlacement());
        }
        return;
    }
    const bool moved = frame.delta.x != 0.0f || frame.delta.y != 0.0f;
    shell_.inputDevice().notePointer(
        frame.delta.x, frame.delta.y,
        !frame.primary.empty() || frame.right || frame.middle || frame.wheel != 0.0f, frame.touch);
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    if (!shell_.inputDevice().pointing() || !frame.point) {
        if (pointerIn_) {
            pointerIn_ = false;
            pointerHeld_ = false;
            navigator.pointer(PointerEvent{PointerKind::Leave, {}, 0.0f});
        }
        return;
    }
    pointerIn_ = true;
    const gpu2d::Point at{frame.point->x, frame.point->y};
    // Only a pointer that moved takes focus, so a keyboard or pad move is not undone by a pointer
    // resting where the focus used to be; a finger only moves while it is down.
    if (moved && (pointerHeld_ || !frame.touch)) {
        navigator.pointer(PointerEvent{PointerKind::Move, at, 0.0f});
    }
    for (const bool down : frame.primary) {
        pointerHeld_ = down;
        navigator.pointer(PointerEvent{down ? PointerKind::Down : PointerKind::Up, at, 0.0f});
    }
    // A right click asks for the menu of what it is on; it is never Back.
    if (frame.right) {
        navigator.pointer(PointerEvent{PointerKind::Context, at, 0.0f});
    }
    if (frame.wheel != 0.0f) {
        navigator.pointer(PointerEvent{PointerKind::Wheel, at, frame.wheel});
    }
}

void ShellApp::selectLauncher(library::Source source) {
    if (shell_.inGame() || !shell_.screens().navigator().atRoot()) {
        return;
    }
    // Selecting the store's tile in Library: from its top, whatever is open now.
    search_.clear();
    if (browser_.section() != library::Section::Library) {
        clickSection(library::Section::Library);
    } else if (const std::optional<std::size_t> focus = browser_.back()) {
        showShelf(*focus);
    }
    const std::vector<ui::Tile>& tiles = shell_.grid().tiles();
    const auto tile = std::ranges::find_if(tiles, [source](const ui::Tile& candidate) {
        const auto* launcher = std::get_if<library::Launcher>(&candidate.item);
        return launcher != nullptr && launcher->source == source;
    });
    if (tile == tiles.end()) {
        shell_.setToast("no " + std::string{library::label(source)} + " library here", true);
        return;
    }
    // input-sound.md 3.4 Navigation: a focus move.
    if (shell_.grid().focusTile(static_cast<std::size_t>(tile - tiles.begin()))) {
        sounds_.play(audio::Effect::Navigation);
    }
    pressTile();
}

void ShellApp::clickSection(library::Section section) {
    if (shell_.inGame() || !shell_.screens().navigator().atRoot()) {
        return;
    }
    moveToSection(section);
}

void ShellApp::moveToSection(library::Section section) {
    const int steps = library::Sections::stepsBetween(browser_.section(), section);
    if (steps != 0) {
        cycleSection(steps);
    }
}

void ShellApp::goToSection(library::Section section) {
    closePages();
    if (section != browser_.section()) {
        moveToSection(section);
        return;
    }
    search_.clear();
    if (const std::optional<std::size_t> focus = browser_.back()) {
        showShelf(*focus);
    }
}

void ShellApp::handleGameKeys() {
    if (GameOverlay* overlay = gameScreen_.overlay()) {
        // The watcher is for a game that has the keyboard; otherwise the shell's window keys are
        // the ones to act on, and acting on both would fire every shortcut twice.
        const bool gameHasKeyboard =
            shell_.inGame() && gameScreen_.mode() != display::OverlayMode::Interactive;
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
    if (mappings_.active()) {
        mappings_.act(button);
        return;
    }
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    if (const std::optional<gpu2d::ui::Action> action =
            ui::interfaceActionOf(button, navigator.typing());
        action && !shortcutEditor_.capturing()) {
        navigator.act(*action);
    }
}

void ShellApp::backOnHome() {
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
}

void ShellApp::cycleSection(int delta) {
    // Search results belong to no section; moving along the dock ends the search.
    if (!search_.clear()) {
        browser_.leave(shell_.grid().focusIndex());
    }
    const std::size_t focus = browser_.cycle(delta);
    shell_.setSection(browser_.section(), true);
    showShelf(focus);
    // input-sound.md 3.3 (`dg3.a`): the cue is sized by how many tiles the section shows at once.
    const std::size_t visible = ui::visibleTiles(
        browser_.section(), preferences_.values().libraryMode, shell_.grid().tiles().size());
    if (const std::optional<audio::Effect> cue = audio::dominoFor(visible)) {
        sounds_.play(*cue);
    }
}

bool ShellApp::launch(const library::Game& game, std::vector<std::string> environment,
                      const std::string& profile) {
    if (!game.installed) {
        panels_.offerInstall(game);
        return false;
    }
    if (remotePlay_.waiting()) {
        shell_.setToast("Still waiting for " + remotePlay_.game().hostName + " to let you in");
        return false;
    }
    if (launches_.running() || remotePlay_.playing()) {
        shell_.setToast("a game is already running", true);
        return false;
    }
    if (game.source == library::Source::Remote) {
        if (!game.hostOnline) {
            shell_.setToast(game.hostName + " is offline; start openSU there", true);
            return false;
        }
        remotePlay_.start(
            game,
            stream::wire::PadMapping{.map = padOwner_.forwarding(mappings_.forGame(game)),
                                     .holdDelay = input::holdDelay},
            window_->importsDmabuf() ? stream::PictureMemory::Dmabuf : stream::PictureMemory::Cpu);
        return true;
    }
    if (game.launch.empty()) {
        if (playWait_.begin(game, environment, profile)) {
            return true;
        }
        shell_.setToast(
            game.unavailable.empty() ? "no way to start " + game.title : game.unavailable, true);
        return false;
    }
    if (saveSharing_.holding()) {
        shell_.setToast("a game is waiting for its saves", true);
        return false;
    }
    const auto held = std::make_shared<const HeldLaunch>(
        HeldLaunch{.game = game, .environment = environment, .profile = profile});
    if (saveSharing_.before(game, profile, [this, held] {
            static_cast<void>(startLaunch(held->game, held->environment, held->profile,
                                          profileRunnerSaves(held->profile)));
        })) {
        shell_.setToast("Syncing " + game.title + "'s saves first");
        return true;
    }
    return startLaunch(game, std::move(environment), profile, profileRunnerSaves(profile));
}

std::optional<library::Game> ShellApp::localGame(const std::string& id) const {
    const auto found = std::ranges::find_if(games_, [&id](const library::Game& game) {
        return game.id == id && game.installed && game.source != library::Source::Remote;
    });
    return found == games_.end() ? std::nullopt : std::optional{*found};
}

bool ShellApp::launchSession(const library::Game& game, const std::filesystem::path& saves) {
    if (launches_.running() || remotePlay_.playing() || saveSharing_.holding()) {
        shell_.setToast("a game is already running", true);
        return false;
    }
    return startLaunch(game, {}, profiles_.book().active().id, saves);
}

std::vector<std::filesystem::path> ShellApp::netplayBiosOf(const library::Game& game) const {
    std::vector<std::filesystem::path> files;
    const std::optional<library::roms::CoreBios> bios = emulatorSettings_.biosOf(game.sourceId);
    if (!bios) {
        return files;
    }
    for (const library::roms::CoreBios::File& file : bios->files) {
        if (file.present && file.used) {
            files.push_back(emulatorSettings_.biosFolder() / file.firmware.path);
        }
    }
    return files;
}

std::map<std::string, std::string> ShellApp::netplayOptionsOf(const library::Game& game) const {
    const std::optional<library::Game> linked = library::netplayGame(game, {});
    if (!linked) {
        return {};
    }
    return settings::resolveCoreOptions(gameSettings_.layers(), library::layerKeysOf(*linked));
}

std::filesystem::path ShellApp::profileRunnerSaves(const std::string& profile) const {
    return retro::profileSaveFolder(profiles_.runnerSaves(), profile);
}

void ShellApp::fetchFrameArt() {
    if (const std::optional<EmulatorDownloads::Progress> running = emulatorDownloads_.progress();
        running && running->id == library::roms::overlayArtId) {
        return;
    }
    if (const std::string refused = emulatorDownloads_.start(library::roms::overlayArtId);
        !refused.empty()) {
        shell_.setToast("The visual preset's frame is not downloaded: " + refused, true);
        return;
    }
    shell_.setToast("Downloading the visual preset's frame; it shows from the next launch", false);
}

bool ShellApp::startLaunch(const library::Game& game, std::vector<std::string> extra,
                           const std::string& profile, const std::filesystem::path& saves) {
    library::Game started = game;
    const library::wine::Prepared wine = wineSettings_.prepare(started);
    if (!wine.refusal.empty()) {
        shell_.setToast(wine.refusal, true);
        return false;
    }
    std::string control = retro_.begin();
    const PreparedRetro prepared = prepareRetroLaunch(
        started, gameSettings_.layers(), emulatorSettings_.catalog(), retro_.settingsFile(),
        emulatorDownloads_.shaderPack().root(), emulatorDownloads_.presets(), saves,
        romsetLists_.dats(), preferences_.values().embeddedRunner && embedsGames());
    if (!prepared.refused.empty()) {
        static_cast<void>(retro_.end());
        shell_.setToast("cannot write " + game.title + "'s settings: " + prepared.refused, true);
        return false;
    }
    if (prepared.artMissing) {
        fetchFrameArt();
    }

    // input-sound.md 3.4 OpenAppRom: every game launch.
    sounds_.play(audio::Effect::OpenAppRom);
    panels_.showLaunch(game.title, game.artwork);
    if (hostedLaunch(started)) {
        relay_.open();
    }
    recordLaunch(game);

    std::optional<library::LaunchSetup> setup = library::launchSetupOf(
        started, emulatorSettings_.catalog(), emulatorDownloads_.setup().wiiControls,
        wiiRemoteSettings_.realRemotes() && device::findWiiRemotes(hidPlaces_).present());
    // A Wii game on real remotes plays them itself: openSU lets them go for it.
    gamepad::HeldPads held = padOwner_.hold(
        mappings_.forGame(game), setup && setup->realWiiRemotes ? gamepad::WiiRemoteHold::ToGame
                                                                : gamepad::WiiRemoteHold::AsPads);
    if (setup) {
        setup->padGuid = held.padGuid;
    }
    std::vector<std::string> environment = std::move(held.environment);
    environment.push_back(std::move(control));
    environment.insert(environment.end(), wine.environment.begin(), wine.environment.end());
    environment.insert(environment.end(), started.launch.environment.begin(),
                       started.launch.environment.end());
    environment.insert(environment.end(), std::make_move_iterator(extra.begin()),
                       std::make_move_iterator(extra.end()));
    // Before the game starts, so it reads the smaller screen as its own.
    presentation_.follow(gameInFront(), &started);
    GameLaunch::Convert convert;
    if (started.launch.conversion) {
        const library::roms::EmulatorEntry* entry = builtInOf(started, emulatorSettings_.catalog());
        if (entry != nullptr) {
            convert = [this, built = *entry](const library::RomConversion& conversion,
                                             std::string& error) {
                return romsetConverter_.build(conversion, built, error);
            };
        }
    }
    const bool running = launches_.start(std::move(started), std::move(environment),
                                         std::move(convert), std::move(setup));
    if (running) {
        playingAs_ = profile;
    }
    // Last, since it replaces the tile `game` may be: this copy may now top its title.
    refreshShelf();
    return running;
}

std::vector<library::Game> ShellApp::copiesOffered(const library::Game& game) const {
    if (library::keepsOwnCopy(browser_, preferences_.values().view)) {
        return {};
    }
    std::vector<library::ShelfItem> shelf;
    for (library::Game& copy : library::TitleCopies{games_}.offered(game)) {
        shelf.emplace_back(std::move(copy));
    }
    covers_.apply(shelf);
    std::vector<library::Game> copies;
    copies.reserve(shelf.size());
    for (library::ShelfItem& item : shelf) {
        copies.push_back(std::move(std::get<library::Game>(item)));
    }
    return copies;
}

void ShellApp::cancelLaunch() {
    playWait_.cancel();
    remotePlay_.cancel();
    launches_.cancel();
}

std::filesystem::path ShellApp::resumeFileOf(const library::Game& game) const {
    return library::saves::runnerResumeFile(profiles_.runnerSaves(), profiles_.book().active().id,
                                            game);
}

std::optional<retro::ResumeHeader> ShellApp::resumeOf(const library::Game& game) const {
    return retro::readResumeHeader(resumeFileOf(game));
}

std::string ShellApp::startFresh(const library::Game& game) const {
    std::string error;
    static_cast<void>(retro::discardResume(resumeFileOf(game), error));
    return error;
}

void ShellApp::openGameSettings(const library::Game& game, EditorLook look, EditorRows more) {
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    navigator.push(std::make_unique<LayeredEditor>(layered_, navigator, shell_.screens().chrome(),
                                                   layered_.forGame(game), look, game.title,
                                                   std::move(more)));
}

std::optional<std::filesystem::path> ShellApp::losslessDll() const {
    return library::findLosslessDll(
        library::steam::Library::discover(resolved_.home, resolved_.steamRoots));
}

std::chrono::nanoseconds ShellApp::presentInterval() const {
    // Generated frames go at the display's rate; the shell itself needs no more than 60.
    const std::optional<display::Output> output = window_->output();
    const int hz = remotePlay_.generating() && output && output->refreshHz > 0
                       ? output->refreshHz
                       : ShellWindow::shellHz;
    return std::chrono::nanoseconds{std::chrono::seconds{1}} / hz;
}

const library::Game* ShellApp::gameInFront() const {
    if (!shell_.inGame() || remotePlay_.streaming()) {
        return nullptr;
    }
    if (focusFollow_.following() && focusFollow_.holder() != display::FocusHolder::Game) {
        return nullptr;
    }
    return &launches_.game();
}

bool ShellApp::menusBlocked() {
    return panels_.active() || paths_.active() || artworkSettings_.typing() || mappings_.active() ||
           (sessionInstall_ && sessionInstall_->active()) ||
           (polkitPrompt_ && polkitPrompt_->asking()) || shortcutEditor_.capturing() ||
           search_.isOpen();
}

void ShellApp::closeRunningGame() {
    gameScreen_.closing();
    launches_.close();
}

void ShellApp::toggleGuide(GuideMenuController::Landing landing) {
    if (guideMenu_.inFront() || !menusBlocked()) {
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

void ShellApp::openGameKeyboard() {
    if (remotePlay_.streaming() || !launches_.running()) {
        shell_.setToast("the keyboard types only into a game running here", true);
        return;
    }
    if (!typesIntoGames(environment_.insideGamescope)) {
        shell_.setToast("the keyboard types into games only inside openSU's Gamescope", true);
        return;
    }
    if (!gameTyping_) {
        try {
            gameTyping_ = openGameTyping(environment_.x11Display);
        } catch (const std::runtime_error& refused) {
            lucent::error("keyboard", "cannot type into the game: {}", refused.what());
            shell_.setToast(std::string{"cannot type into the game: "} + refused.what(), true);
            return;
        }
    }
    keyboardPad_.openOverGame(*gameTyping_);
}

void ShellApp::syncOverlay() {
    using display::OverlayMode;
    shell_.followChordHint(router_.longHeld(std::chrono::steady_clock::now()));
    // A screen over Home, the keyboard or the mapping editor takes the buttons; the keyboard over a
    // game leaves it the keys it types.
    const bool typing = keyboardPad_.overGame();
    const bool interactive = shell_.screens().navigator().active() ||
                             (shell_.keyboard().isOpen() && !typing) ||
                             shell_.mappingPage().isOpen();
    const OverlayMode over = interactive                 ? OverlayMode::Interactive
                             : typing                    ? OverlayMode::Typing
                             : shell_.chordHintVisible() ? OverlayMode::Passive
                                                         : OverlayMode::Hidden;
    gameScreen_.show(gameScreen_.running() ? over : OverlayMode::Hidden);
}

std::string ShellApp::outputLine() const {
    const std::optional<display::Output> output = window_ ? window_->output() : std::nullopt;
    if (!output) {
        return {};
    }
    return std::to_string(output->width) + " x " + std::to_string(output->height) + " at " +
           std::to_string(output->refreshHz) + " Hz";
}

void ShellApp::serviceDevices(std::chrono::steady_clock::time_point now) {
    if (bluetooth_) {
        if (bluetooth_->poll(now, devicesScreen_.showsBluetooth())) {
            devicesScreen_.refresh();
        }
        if (const std::optional<BluetoothNotice> notice = bluetooth_->takeNotice()) {
            shell_.setToast(notice->text, notice->error);
        }
    }
    devicesScreen_.tick(now);
}

void ShellApp::setHidden(const library::Game& game, bool hidden) {
    preferences_.values().hidden.set(game, hidden);
    preferences_.save();
    refreshShelf();
    shell_.setToast(game.title + (hidden ? " hidden" : " shown again"));
}

std::optional<bool> ShellApp::onHome(const library::ShelfItem& item) const {
    return library::onHome(games_, item, preferences_.values().home);
}

void ShellApp::setOnHome(const library::ShelfItem& item, bool on) {
    library::setOnHome(games_, item, on, preferences_.values().home);
    preferences_.save();
    refreshShelf();
    const auto* game = std::get_if<library::Game>(&item);
    const std::optional<library::Folder> folder = library::folderOf(item);
    const std::string name = game != nullptr ? game->title : folder ? library::name(*folder) : "";
    shell_.setToast(name + (on ? " added to Home" : " removed from Home"));
}

void ShellApp::recordLaunch(const library::Game& game) {
    preferences_.values().lastPlayed.record(game.id, std::chrono::system_clock::now());
    preferences_.values().lastPlayed.apply(games_);
    preferences_.save();
}

void ShellApp::shareLibrary() {
    lan_.share(covers_.shown(games_));
}

void ShellApp::serveGuests() {
    lan::CoverDesk* desk = lan_.coverDesk();
    if (desk == nullptr) {
        return;
    }
    covers_.serve(*desk);
    for (const lan::wire::GameMatch& match : desk->takeMatches()) {
        if (const library::Game* game = ownGame(match.game)) {
            keepMatch(*game, match.key, match.title);
        }
    }
}

void ShellApp::keepMatch(const library::Game& game, const std::string& key,
                         const std::string& title) {
    if (game.source == library::Source::Remote) {
        const bool asked = lan_.askPeers(
            [host = game.hostId, match = lan::wire::GameMatch{game.sourceId, key, title}](
                const lan::LanNode& node, const std::stop_token& stop) {
                std::string error;
                if (!node.matchGame(stop, host, match, error)) {
                    lucent::warn("match", "{}: telling its host: {}", match.game, error);
                }
            });
        shell_.setToast(asked ? game.title + " is now matched to " + title
                              : "Cannot reach " + game.hostName,
                        !asked);
        return;
    }
    gameSettings_.layers().set(settings::Layer{settings::Layer::Kind::Game, library::dataId(game)},
                               library::matchKey, key);
    gameSettings_.save();
    shell_.setToast(game.title + " is now matched to " + title, false);
    // The source's own art comes back with a fresh read, for the match to replace.
    reloadCatalog();
}

std::optional<library::Game> ShellApp::shownGame(const std::string& id) const {
    const library::Game* own = ownGame(id);
    // A title's tile moves to whichever copy tops it.
    const std::vector<library::Game> copies =
        own == nullptr ? std::vector<library::Game>{} : library::copiesOf(games_, *own);
    for (const ui::Tile& tile : shell_.grid().tiles()) {
        const auto* game = std::get_if<library::Game>(&tile.item);
        if (game == nullptr ||
            (game->id != id &&
             std::ranges::find(copies, game->id, &library::Game::id) == copies.end())) {
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
        panels_.active()) {
        return;
    }
    // The details page, the Settings screen and the Devices page are the only panels the trail
    // stays clickable over.
    details_.close();
    const bool settingsAlone =
        settingsScreen_.isOpen() && !paths_.active() && !shell_.keyboard().isOpen();
    if (shell_.screens().navigator().active() && !settingsAlone && !devicesScreen_.isOpen()) {
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
    if (!shell_.screens().navigator().atRoot()) {
        return ui::HintContext{};
    }
    if (shortcutEditor_.capturing()) {
        return ui::HintContext{.back = true};
    }
    const ui::HomeGrid& grid = shell_.grid();
    const bool game = grid.focusedGame() != nullptr;
    const bool folder = grid.focusedFolder().has_value();
    return ui::HintContext{.back = browser_.canBack(),
                           .clearSearch = search_.searching(),
                           .select = game || folder || grid.focusedGiveaways() ||
                                     grid.focusedAdd() != nullptr,
                           .details = game,
                           .options = game || folder,
                           .menu = true};
}

void ShellApp::syncChrome() {
    const ui::HintContext corner = hints();
    shell_.setHints(corner);
    shell_.setLaunchers(launcherBadges(steam_.state(), steam_.downloads(), sources_));
    const gpu2d::ui::Navigator& navigator = shell_.screens().navigator();
    const bool home = navigator.atRoot() && !shell_.inGame();
    // iiSU's Home has no title (pl3.q); opensu's names the focused tile, except over a screen,
    // whose trail names where it is.
    shell_.setTitle(navigator.atRoot() ? shell_.grid().pillTitle() : std::string{});
    shell_.setTrail(currentTrail());
    const bool free = !shell_.keyboard().isOpen() && !shell_.mappingPage().isOpen();
    // A finger has no B or Guide: while a pointer leads, the top bar offers both.
    const bool touch = shell_.inputDevice().pointing() && free;
    shell_.setTouchButtons(ui::Hud::TouchButtons{
        .back = touch && (!navigator.atRoot() || corner.back || corner.clearSearch),
        .menu = touch && (guideMenu_.inFront() || !menusBlocked()),
        .prompts = touch});
    shell_.topBar().setLive(ui::TopBarChrome::Live{
        .crumbs = !shell_.inGame() && free, .launchers = home && free, .buttons = touch});
    shell_.cornerHints().setLive(free);
}

void ShellApp::publishSnapshot() {
    ShellSnapshot next;
    next.games = games_.size();
    next.installed = installedCount_;
    const ui::HomeGrid& grid = shell_.grid();
    next.artworkPending = grid.artworkPending();
    next.imagesPending = shell_.screens().images().pending();
    next.worstFrameUs = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(frameTiming_.worst()).count());
    if (const library::Game* focused = grid.focusedGame(); focused != nullptr) {
        next.focusedId = focused->id;
    } else if (const std::optional<library::Folder> folder = grid.focusedFolder()) {
        next.focusedId = std::holds_alternative<library::Console>(*folder)
                             ? "console:" + library::key(*folder)
                             : library::key(*folder);
    } else if (grid.focusedGiveaways()) {
        next.focusedId = "giveaways";
    } else if (const library::AddTile* add = grid.focusedAdd()) {
        next.focusedId = add->id;
    }
    next.focusedTitle = grid.focusedTitle();
    next.section = std::string{library::key(browser_.section())};
    next.libraryMode = std::string{library::key(preferences_.values().libraryMode)};
    next.searchOpen = search_.isOpen();
    next.searchText = preferences_.values().view.search;
    next.contextMenuOpen = contextMenu_.isOpen();
    next.detailsOpen = details_.isOpen();
    next.settingsOpen = settingsScreen_.isOpen();
    // Home is no screen to the tools that read this.
    if (const gpu2d::ui::Navigator& navigator = shell_.screens().navigator(); !navigator.atRoot()) {
        next.screen = navigator.top()->id();
        next.screenFocus = navigator.focused();
    }
    next.nightLightKelvin = static_cast<std::size_t>(nightLight_.kelvin());
    next.nightLightShown = nightLight_.active();
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
    next.iconSize = static_cast<std::size_t>(grid.iconSize());
    next.shelf = browser_.folder() ? library::key(*browser_.folder()) : next.section;
    next.focusIndex = grid.focusIndex();
    next.page = static_cast<std::size_t>(std::max(grid.page(), 0));
    next.pageCount = static_cast<std::size_t>(std::max(grid.pageCount(), 1));
    next.status = shell_.status();
    next.toast = shell_.toast();
    next.toastIsError = shell_.toastIsError();
    next.launching = launches_.running();
    // What the loop has applied, not what was asked for, so a request that never reached the
    // loop cannot read as in game.
    next.inGame = shell_.inGame();
    next.guideMenuOpen = guideMenu_.isOpen();
    next.devicesOpen = devicesScreen_.isOpen();
    next.devicesTab = devicesScreen_.tab();
    next.downloadsOpen = downloadsScreen_.isOpen();
    next.downloads = downloadsScreen_.lines();
    next.steam = std::string{launch::name(steam_.state())};
    next.inputDevice = std::string{input::name(shell_.inputDevice().current())};
    next.launchers = describe(launcherBadges(steam_.state(), steam_.downloads(), sources_));
    const LanService::Glance lan = lan_.glance();
    next.lanOn = lan.on;
    next.lanSeen = lan.seen;
    next.lanPaired = lan.paired;
    next.lanCode = lan.code;
    next.lanImport = lanRoms_.glance();
    next.onlineStep = onlinePlay_.glance();
    // A code is a bearer secret: only a hidden run's tools read it off the channel.
    next.onlineCode = settings_.hidden ? onlinePlay_.code() : std::string{};
    if (const ui::GameDetails* shown = details_.details()) {
        if (const ui::SettingsRow* saves = ui::factOf(*shown, "Saves")) {
            next.lanSaves = saves->value;
        }
    }
    next.profile = profiles_.book().active().name;
    next.profiles = profiles_.book().all().size();
    const RemotePlay::Glance remote = remotePlay_.glance();
    next.remotePlay = remote.state;
    next.remotePeer = remote.peer;
    next.remoteGuests = remote.guests;
    next.remoteHeld = remote.held;
    next.remoteAsk = remote.asking;
    next.remoteRoundTripUs = remote.roundTripUs;
    next.remoteBytesPerSecond = remote.bytesPerSecond;
    next.remoteVideo = remote.video;
    next.remoteAudio = remote.audio;

    bridge_.publish(std::move(next));
}

void ShellApp::serviceControlRequests() {
    // Buttons injected over the channel take the same path as a real press, so
    // what the channel exercises is the shell's own handling.
    const ControlBridge::Input queued = bridge_.takeInput(std::chrono::steady_clock::now());
    // Keys and text go to the window's event queue and come back as the keyboard's next frames.
    for (const ControlBridge::Keystroke& stroke : queued.keystrokes) {
        if (const auto* combo = std::get_if<input::Combo>(&stroke)) {
            window_->press(*combo);
        } else {
            window_->type(std::get<std::string>(stroke));
        }
    }
    // The pointer's too, so a posted button is held across frames as the mouse's is.
    for (const PointerInjection& injected : queued.pointer) {
        postPointer(injected);
    }
    if (queued.linkCap) {
        capRemoteLink(*queued.linkCap);
    }
    if (queued.clipboard) {
        window_->setClipboard(*queued.clipboard);
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

    const std::optional<FrameKind> wanted = bridge_.frameWanted();
    if (!wanted) {
        return;
    }
    if (*wanted == FrameKind::Remote) {
        bridge_.answerFrame(remotePicture_->pixels(remotePlay_.picture()));
        return;
    }
    bridge_.answerFrame(FramePixels{.pixels = renderShell(*window_, shell_), .number = 0});
}

void ShellApp::postPointer(const PointerInjection& injected) {
    using Kind = PointerInjection::Kind;
    const auto sdlButton = [](PointerInjection::Button button) -> std::uint8_t {
        switch (button) {
        case PointerInjection::Button::Middle:
            return SDL_BUTTON_MIDDLE;
        case PointerInjection::Button::Right:
            return SDL_BUTTON_RIGHT;
        case PointerInjection::Button::Left:
            break;
        }
        return SDL_BUTTON_LEFT;
    };
    const ui::Point at{injected.x, injected.y};
    switch (injected.kind) {
    case Kind::Move:
        window_->movePointer(at);
        break;
    case Kind::Click:
        window_->pressPointer(at, SDL_BUTTON_LEFT, true);
        window_->pressPointer(at, SDL_BUTTON_LEFT, false);
        break;
    case Kind::Tap:
        window_->tapPointer(at);
        break;
    case Kind::Down:
    case Kind::Up:
        window_->pressPointer(at, sdlButton(injected.button), injected.kind == Kind::Down);
        break;
    case Kind::Wheel:
        window_->turnWheel(at, injected.wheel);
        break;
    }
}

void ShellApp::capRemoteLink(std::uint64_t bitsPerSecond) {
    if (!settings_.hidden) {
        lucent::warn("remote", "a link cap is for hidden runs; {} bit/s is not applied",
                     bitsPerSecond);
        return;
    }
    remotePlay_.capLink(bitsPerSecond);
}

bool ShellApp::remoteHasControls() {
    return remotePlay_.showing() && !guideMenu_.isOpen() && !menusBlocked() &&
           !shell_.screens().navigator().active();
}

void ShellApp::sendRemoteInput() {
    if (!remotePlay_.streaming()) {
        return;
    }
    if (!remoteHasControls()) {
        remoteInput_.release();
        const RemoteInput::State released = remoteInput_.take();
        remotePlay_.input(released.pad, {}, released.mouse);
        return;
    }
    remoteInput_.axes(pads_->axes());
    stream::wire::KeyState keys;
    const std::vector<std::uint8_t>& held = window_->keysHeld();
    keys.held.assign(held.begin(), held.begin() + static_cast<std::ptrdiff_t>(std::min(
                                                      held.size(), stream::wire::heldKeys)));
    const RemoteInput::State state = remoteInput_.take();
    remotePlay_.input(state.pad, keys, state.mouse);
}

display::Placed ShellApp::remotePlacement() const {
    const gpu2d::PixelSize window = window_->size();
    return remotePicture_->placement({.width = window.width, .height = window.height},
                                     static_cast<int>(window_->contentInsets().top),
                                     remoteTouch_->band());
}

void ShellApp::showGamePicture() {
    const std::chrono::nanoseconds refresh = presentInterval();
    window_->presentEvery(refresh);
    if (!remotePlay_.streaming()) {
        remotePicture_->clear();
        const GameOverlay* overlay = gameScreen_.overlay();
        gameBackdrop_->follow(overlay != nullptr && !overlay->seesGame() &&
                                  gameScreen_.mode() == display::OverlayMode::Interactive,
                              retro_);
        shell_.setPicture(gameBackdrop_->view());
        shell_.setPictureBand(0);
        shell_.setPictureControls(std::nullopt);
        return;
    }
    const gpu2d::PixelSize window = window_->size();
    remotePlay_.showsIn(window.width, window.height);
    if (std::optional<stream::GuestVideo::Frame> frame = remotePlay_.takePicture(
            stream::wire::nowMicros(), static_cast<std::uint64_t>(refresh.count()))) {
        if (!remotePicture_->show(*frame)) {
            remotePlay_.readPicturesBack();
        }
    }
    remotePicture_->shapeAs(remotePlay_.shape());
    shell_.setPicture(remotePicture_->view());
    const std::optional<bool> nearest = remotePlay_.nearest();
    shell_.setPictureShape(
        remotePlay_.shape(),
        nearest ? std::optional{*nearest ? gpu2d::Filter::Nearest : gpu2d::Filter::Linear}
                : std::nullopt);
    shell_.setPictureBand(remoteTouch_->band());
    shell_.setPictureControls(remoteTouch_->view());
}

/// Applies what the launch thread and the control channel asked for. Everything
/// that touches the window or the shell happens here, on the loop's thread.
void ShellApp::serviceRequests() {
    const bool launching = launches_.running();
    saveSharing_.playing(launching ? &launches_.game() : nullptr);
    if (const std::optional<library::Game> ended = launches_.takeEnded()) {
        layered_.keepScreenTurn(*ended);
        lanNetplay_.ended(*ended);
        netplayRun_.ended(*ended);
        onlinePlay_.ended();
        saveSharing_.ended(*ended, playingAs_);
    }
    if (padOwner_.held() && !launching) {
        padOwner_.release();
    }
    if (const std::optional<launch::LaunchProgress> progress = mailbox_.takeProgress()) {
        const bool measured = progress->stage == launch::LaunchProgress::Stage::Updating;
        panels_.update(launch::describe(*progress),
                       measured ? std::optional{progress->fraction} : std::nullopt);
        remotePlay_.hostStage(launch::describe(*progress));
    }
    playWait_.service();
    // Here, not in the wait's hook: a guest's launch starts the wait under the host's lock.
    if (std::optional<std::string> line = playWait_.newLine()) {
        remotePlay_.hostStage(std::move(*line));
    }
    const std::string remoteStage = remotePlay_.stage();
    if (!remoteStage.empty()) {
        if (!panels_.active()) {
            panels_.showLaunch(remotePlay_.game().title, remotePlay_.game().artwork);
        }
        panels_.update(remoteStage, std::nullopt);
    }
    if (!launching && !playWait_.waiting() && !launches_.running() && remoteStage.empty()) {
        panels_.endLaunch();
        relay_.close();
    }
    panels_.service();
    if (const std::optional<std::string> reload = bridge_.takeReload()) {
        if (!reload->empty()) {
            shell_.setToast(*reload);
        }
        reloadCatalog();
    }
    const bool running = mailbox_.gameRunning();
    // A remote game plays in this window, so only a game here moves the window aside.
    if (running != gameScreen_.running()) {
        gameScreen_.setRunning(running);
    }
    relay_.follow(running, display::takesPads(gameScreen_.mode()), pads_->connected().size(),
                  window_->touchSurface());
    if (const bool inGame = running || remotePlay_.showing(); inGame != shell_.inGame()) {
        shell_.setInGame(inGame);
        closePages();
        panels_.endLaunch();
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
    // Read before any game makes the screen smaller.
    if (const std::optional<display::Output> output = window_->output()) {
        presentation_.setScreen(settings::ScreenSize{output->width, output->height});
    }
    // A hidden shell inside Gamescope takes the same overlay and focus path; only its window is
    // unmapped.
    if (session_) {
        // Spanning the screen also draws the home screen at the output's own resolution.
        if (session_->coversMonitor()) {
            const gpu2d::PixelSize size = window_->fillMonitor();
            shell_.setSize(size.width, size.height);
        }
        gameScreen_.attachOverlay();
        focusFollow_.attach(session_->focusSource(), window_->x11Window());
    }
    if (settings_.playSounds) {
        sounds_.open();
    }

    // The control channel is part of the product, not a debug flag: it is how an
    // automated run drives the shell without a controller.
    if (settings_.controlChannel) {
        control_ = std::make_unique<ControlChannel>(bridge_, signIn_, settings_.controlPort);
        control_->start();
    }

    // Steam comes up in the background, in its mode, while the shell is already usable.
    integrationSwitch_.begin();

    while (!bridge_.closeRequested() && window_->pollEvents()) {
        frameTiming_.start(std::chrono::steady_clock::now());
        // A resize changes the framebuffer, so the layout is recomputed before anything is drawn.
        if (const std::optional<gpu2d::PixelSize> size = window_->takeResize()) {
            shell_.setSize(size->width, size->height);
        }
        shell_.setDisplayDp(window_->displayDp());
        shell_.setSafeArea(window_->contentInsets());
        // Files added to a ROM folder while openSU was away show when it comes back.
        if (window_->takeForeground()) {
            lucent::info("catalog", "back in the foreground; listing the library again");
            reloadCatalog();
        }

        integrationSwitch_.service();
        padOwner_.lendToSteam(steamGridDbAccount_.signingIn() || offerClaim_.inSteam());
        focusFollow_.service(padOwner_.held());
        // Between a game's windows Gamescope focuses the shell's window, which then draws its own.
        shell_.setStandsIn(remotePlay_.streaming() ||
                           (shell_.inGame() && focusFollow_.following() &&
                            focusFollow_.holder() == display::FocusHolder::OpenSU));
        presentation_.follow(gameInFront(), launches_.running() ? &launches_.game() : nullptr);
        nightLight_.service(preferences_.values().nightLight, std::chrono::system_clock::now());
        padOwner_.follow(focusFollow_.following(), focusFollow_.holder(),
                         display::takesPads(gameScreen_.mode()));
        handlePads();
        handleKeyboard();
        handlePointer();
        handleGameKeys();
        volume_.poll(std::chrono::steady_clock::now());
        paths_.poll();
        if (const auto direction = repeat_.poll(std::chrono::steady_clock::now())) {
            actOn(*direction);
        }

        serviceControlRequests();
        sendRemoteInput();
        serviceRequests();
        serviceCatalog();
        plugins_.service();
        lan_.service();
        lanRoms_.service();
        lanProfiles_.service();
        lanNetplay_.service();
        onlinePlay_.service();
        saveSharing_.service();
        remotePlay_.service(launches_.running() || saveSharing_.holding() || playWait_.waiting(),
                            launches_.running() ? &launches_.game() : nullptr, playingAs_,
                            mailbox_.gameRunning());
        togetherQuestions_.service();
        delivery_.service();
        covers_.service();
        serveGuests();
        serviceDevices(std::chrono::steady_clock::now());
        if (sessionInstall_) {
            sessionInstall_->service();
        }
        if (polkitPrompt_) {
            polkitPrompt_->service();
        }
        emulatorDownloads_.service();
        wineRunners_.service(std::chrono::steady_clock::now());
        downloadsScreen_.service();
        settingsScreen_.service();
        giveaways_.service();
        ownGames_.service();
        gameMatch_.service();
        saveCopy_.service();
        guideMenu_.service();
        if (window_->screenKeyboardHidden()) {
            keyboardPad_.close();
        }
        keyboardPad_.follow(shell_.inGame());
        window_->showScreenKeyboard(keyboardPad_.systemTyping());
        syncOverlay();
        syncChrome();
        sounds_.playAsked();
        menuMusic_.service(interfaceThemeSettings_.current(),
                           MenuMusic::wanted(preferences_.values(), shell_.inGame()));
        shell_.tick(std::chrono::steady_clock::now());
        publishSnapshot();
        // An unmapped window draws nothing, and its artwork still streams in.
        shell_.loadArtwork();
        showGamePicture();
        const ShellWindow::Presented presented =
            window_->present([this](ui::Canvas& canvas, const gpu2d::Surface& target) {
                shell_.draw(canvas, target);
            });
        if (const std::optional<std::uint32_t> shown = remotePicture_->takeFresh()) {
            remotePlay_.presented(*shown, stream::wire::microsOf(presented.submitted),
                                  presented.drawn);
        }
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
        window_ = std::make_unique<ShellWindow>(ShellWindow::Options{
            title, settings_.width, settings_.height, hidden, settings_.scale});
        shell_.attach(window_->device());
        remotePicture_ =
            std::make_unique<RemotePicture>(window_->device(), window_->importsDmabuf());
        remoteTouch_ =
            std::make_unique<RemoteTouch>(window_->device(), environment_.assetsDir / "touch");
        gameBackdrop_ = std::make_unique<GameBackdrop>(window_->device());
    } catch (const gpu2d::GpuError& error) {
        lucent::error("shell", "could not open a window: {}", error.what());
        window_.reset();
        return false;
    }
    gameScreen_.attach(*window_);
    keyboardPad_.setSystemKeyboard(window_->hasScreenKeyboard());
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
    lucent::info("render", "loaded artwork for {} of {} tiles", shell_.grid().loadedArtwork(),
                 shell_.grid().tiles().size());
    // A still frame shows the grid at rest, after its entrance.
    shell_.tick(std::chrono::steady_clock::now());
    syncChrome();
    shell_.settle();

    const gpu2d::image::Bitmap frame = renderShell(*window_, shell_);
    std::string error;
    const std::string png = gpu2d::image::encodePng(frame.rgba, static_cast<unsigned>(frame.width),
                                                    static_cast<unsigned>(frame.height), error);
    if (png.empty()) {
        lucent::error("render", "could not encode the frame: {}", error);
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
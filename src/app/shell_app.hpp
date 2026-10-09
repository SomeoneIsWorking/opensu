// app — composition. Owns the catalog, the controller reader and the drawn
// shell, and is the only place the two halves meet.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "raylib.h"

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "audio/sound_player.hpp"
#include "breadcrumb_trail.hpp"
#include "config/config.hpp"
#include "context_menu_controller.hpp"
#include "control_channel.hpp"
#include "details_controller.hpp"
#include "device/battery.hpp"
#include "game_keys.hpp"
#include "gamepad/direction_repeat.hpp"
#include "gamepad/pads.hpp"
#include "gamescope_overlay.hpp"
#include "gamescope_windows.hpp"
#include "input/keyboard_bindings.hpp"
#include "launch/handoff.hpp"
#include "layout_picker.hpp"
#include "library/catalog.hpp"
#include "library/catalog_loader.hpp"
#include "library/shelf.hpp"
#include "panel_flow.hpp"
#include "pointer_router.hpp"
#include "preferences.hpp"
#include "search_controller.hpp"
#include "sign_in.hpp"
#include "steam/client.hpp"
#include "ui/shell.hpp"

namespace opensu::app {

/// What the shell needs from the host, so the shell can be drawn without a
/// running store client.
struct Settings {
    int width{1280};
    int height{800};
    /// Where the loopback control channel listens. Zero asks for a free port.
    std::uint16_t controlPort{0};
    /// Whether the control channel runs. It is how an automated run drives the
    /// shell, so it is on unless configuration turns it off.
    bool controlChannel{true};
    /// The home grid's dashboard mode.
    config::HomeMode homeMode{config::HomeMode::Standard};
    /// The window is never mapped and no pad is read, for maintainer runs and tests.
    bool hidden{false};
};

/// The running shell.
class ShellApp final : public ControlTarget, private PointerHost {
  public:
    explicit ShellApp(const Settings& settings);

    /// Loads the library, then runs the window loop until it closes.
    int run();

    /// Renders one frame offscreen and writes it to `path`, without opening a
    /// window. This is what makes the layout checkable from a test.
    bool renderToFile(const std::string& path, bool keyboardPrompts = false);

    /// The catalog as last read.
    [[nodiscard]] const std::vector<library::Game>& games() const noexcept {
        return games_;
    }

    // ControlTarget. Each is callable from another thread, and each hands work to
    // the main loop, because the OpenGL context and the shell's state belong to
    // it and to no other thread.
    [[nodiscard]] ShellSnapshot snapshot() const override;
    void inject(gamepad::Button button, input::Device device) override;
    [[nodiscard]] bool captureFrame(std::string& png) override;
    void requestClose() override;
    void requestCatalogReload(std::string toast) override;
    void typeText(std::string text) override;

  private:
    /// Where pads are read from: the system's, or an empty directory in a hidden run.
    static std::filesystem::path padsDirectory(bool hidden);

    /// Publishes state for the control channel, and serves any pending request.
    /// Main loop only.
    void publishSnapshot();
    void serviceControlRequests();
    void serviceRequests();

    /// Requests raised by the launch thread and applied by the main loop, because
    /// the GL context and the shell's state belong to it. Each of these only sets
    /// a flag: none of them touches the window or the shell from that thread.
    void requestGameRunning(bool running);
    void requestToast(std::string text, bool isError);
    /// Hands the loop a launch's progress, for the launch panel. Any thread.
    void requestLaunchProgress(const launch::LaunchProgress& progress);

    /// Asks every store to list again; the listings reach the shell as they arrive.
    void reloadCatalog();
    /// Takes the listings that arrived and shows them. Main loop only.
    void serviceCatalog();
    /// Lists every store and waits for all of them; for a still render only.
    void loadCatalogNow();
    void applyCatalog(library::CatalogSnapshot snapshot);
    void handleEvents(const std::vector<gamepad::Event>& events);
    /// Maps held keys onto the buttons they stand in for, so the keyboard reaches
    /// the same actions a controller does rather than a parallel set.
    void handleKeyboard();
    /// Keyboard shortcuts pressed while a game has the keyboard (Shift+Tab is Guide).
    void handleGameKeys();
    /// Pad events, noting the pad as the device in use.
    void handlePads();
    /// The pointer: routed onto the shell's actions. Main loop only.
    void handlePointer();
    // PointerHost.
    ui::PointerTarget pointAt(std::optional<Vector2> point) override;
    void focus(const ui::PointerTarget& target) override;
    void press(gamepad::Button button) override;
    void activateSection(library::Section section) override;
    void selectLauncher(library::Source source) override;
    void activateCrumb(std::size_t index) override;
    void scroll(int steps) override;
    void chooseIconSize(int level) override;
    void contextMenu(const ui::PointerTarget& target) override;
    /// A click on a dock item: the same section change as L1 and R1.
    void clickSection(library::Section section);
    void actOn(gamepad::Button button);
    /// Moves home focus, with iiSU's Navigation sound when it moved.
    void moveFocus(ui::Direction direction);
    /// Moves the Guide menu's focus, with the Navigation sound when it moved.
    void moveMenu(int delta);
    /// L1 and R1: moves `delta` sections along the dock and shows the section's shelf, with iiSU's
    /// domino cue sized by what the section shows at once.
    void cycleSection(int delta);
    /// Gives the shell the dock icons the store holds. Needs no GL context.
    void loadStoredNavIcons();
    /// Launches `game`, or offers to install it.
    void launch(const library::Game& game);
    /// Abandons a launch whose game has not appeared yet, such as one waiting on a Steam update.
    void cancelLaunch();
    /// Asks whether to install the focused game, which is not installed, from the stores that
    /// can: all the stores that own it when more than one can.
    void offerInstall(const library::Game& game);
    /// Shows the shelf the browser is on, focusing `focus`.
    void showShelf(std::size_t focus);
    /// The folder open now as the tile that stands for it, or nothing in a section.
    [[nodiscard]] std::optional<library::ShelfItem> folderCard() const;
    /// Shows artwork and loads sounds the fetcher has downloaded. Main loop only.
    void serviceArtwork();
    /// Loads every UI sound the store holds. Main loop only, once the audio device is open.
    void loadStoredSounds();
    /// Opens a console, a launcher or the combined library on its games.
    void openFolder(const library::Folder& folder);
    /// Gives the screen the layout, pin and icon size the preferences hold.
    void applyLayout();
    /// The tiles the grid shows, as lines of the search results.
    [[nodiscard]] std::vector<ui::SearchResult> searchResults() const;
    /// Focuses tile `index` and presses A on it.
    void openTile(std::size_t index);
    /// Hides or shows `game` everywhere, and keeps it.
    void setHidden(const library::Game& game, bool hidden);
    /// Notes that `game` was launched now, for the recently played sort.
    void recordLaunch(const library::Game& game);
    /// Reads the keyboard as text while the search panel is open. `typed` was read before the
    /// frame's key bindings ran.
    void handleSearchText(const input::TextInput& typed);
    /// Opens a store's sign-in page in the browser, off the loop.
    void startSignIn(Store store);
    /// Buttons while a game runs: Guide opens and closes the menu over it, which takes the
    /// rest. Main loop only.
    void actInGame(gamepad::Button button);
    /// Opens or closes the Guide menu and shows or hides the window drawing it.
    void setGameMenuOpen(bool open);
    /// Gives the shell what its chrome shows now: prompts, launcher badges, title and trail.
    void syncChrome();
    /// The trail the top bar shows for where the shell is now.
    [[nodiscard]] ui::Trail currentTrail() const;
    /// Runs ROM `game` on emulator `name` from now on, and keeps the pick.
    void chooseEmulator(const library::Game& game, const std::string& name);
    /// The game with `id` as the grid shows it now.
    [[nodiscard]] std::optional<library::Game> shownGame(const std::string& id) const;
    /// What each button does now, for the corner prompts: the same tests the actions make.
    [[nodiscard]] ui::HintContext hints() const;
    /// Re-reads the clock and the battery and schedules the next minute boundary.
    void refreshClock();
    void pushCatalogToShell();
    /// The games whose art the shell wants first, as last sent to the fetcher.
    std::vector<std::string> prioritised_;

    Settings settings_;
    /// Lists every store off the loop; the loop takes what has arrived.
    library::CatalogLoader catalogLoader_;
    /// Downloaded artwork, and the worker that fills it in.
    artwork::ArtworkStore artworkStore_{config::read().cacheDir / "artwork"};
    artwork::ArtworkFetcher artworkFetcher_{artworkStore_, artwork::RemoteSources{},
                                            library::roms::NameDb::under(config::read().cacheDir)};
    /// iiSU's UI sounds, played from the loop's input handling.
    audio::SoundPlayer sounds_;
    device::BatteryReader battery_;
    /// When the clock next changes, on the minute boundary.
    std::chrono::steady_clock::time_point nextClockTick_{};
    ui::Shell shell_;
    /// What the player chose, read at start and saved when it changes.
    Preferences preferences_{settings::Store{config::read().configDir / "settings.json"},
                             [this](const std::string& why) {
                                 shell_.setToast(why, true);
                             }};
    /// After the preferences they keep and the shelf they re-show.
    LayoutPicker layoutPicker_{shell_.modeChooser(), sounds_, preferences_,
                               LayoutPicker::Hooks{[this](std::size_t focus) {
                                                       showShelf(focus);
                                                   },
                                                   [this] {
                                                       return shell_.focusIndex();
                                                   },
                                                   [this] {
                                                       applyLayout();
                                                   },
                                                   [this] {
                                                       return library::sourceChoices(games_,
                                                                                     sources_);
                                                   },
                                                   [this] {
                                                       search_.open();
                                                   }}};
    SearchController search_{shell_.searchPanel(), sounds_, preferences_,
                             SearchController::Hooks{[this](std::size_t focus) {
                                                         showShelf(focus);
                                                     },
                                                     [this] {
                                                         return searchResults();
                                                     },
                                                     [this](std::size_t index) {
                                                         openTile(index);
                                                     }}};
    ContextMenuController contextMenu_{
        shell_.contextMenu(), sounds_, preferences_,
        ContextMenuController::Hooks{[this] {
                                         return shell_.focusedItem();
                                     },
                                     [this] {
                                         if (const library::Game* game = shell_.focusedGame()) {
                                             launch(*game);
                                         }
                                     },
                                     [this] {
                                         details_.open();
                                     },
                                     [this](const library::Game& game, bool hidden) {
                                         setHidden(game, hidden);
                                     },
                                     [this](const library::Folder& folder) {
                                         openFolder(folder);
                                     },
                                     [this] {
                                         reloadCatalog();
                                         shell_.setToast("library refreshed");
                                     },
                                     [this](library::Source source) {
                                         startSignIn(source == library::Source::Gog ? Store::Gog
                                                                                    : Store::Epic);
                                     }}};
    DetailsController details_{
        shell_.detailsPage(), sounds_, preferences_,
        DetailsController::Hooks{[this] {
                                     return shell_.focusedGame();
                                 },
                                 [this](const std::string& id) {
                                     return shownGame(id);
                                 },
                                 [this](const library::Game& game) {
                                     launch(game);
                                 },
                                 [this](const library::Game& game, bool hidden) {
                                     setHidden(game, hidden);
                                 },
                                 [this](const library::Game& game, const std::string& name) {
                                     chooseEmulator(game, name);
                                 }}};
    gamepad::Pads pads_{padsDirectory(settings_.hidden)};
    /// After shell_ and the host it drives.
    PointerRouter pointer_{shell_.inputDevice(), *this};
    /// Held directions, from a pad or the keyboard, repeat on one schedule.
    gamepad::DirectionRepeat repeat_;
    /// The pads are held for the running launch. Main loop only.
    bool padsHeld_{false};

    /// Created after construction, because it drives this shell.
    std::unique_ptr<ControlChannel> control_;

    std::vector<library::Game> games_;
    /// How many of `games_` are installed, counted when the catalog arrives.
    std::size_t installedCount_{0};
    /// Each store's state from the last catalog read.
    std::vector<library::SourceStatus> sources_;
    /// Home or the open folder. Main loop only.
    library::ShelfBrowser browser_;
    /// Guards the handoff thread, which touches the window.
    std::mutex launchMutex_;
    bool launchRunning_{false};
    /// Started in run(), before the loop, and shut down with the app.
    steam::Client steam_;
    /// Inside Gamescope, tells the handoff when a game shows a window. Null elsewhere.
    std::unique_ptr<session::GamescopeWindows> gameWindows_;
    launch::Handoff handoff_;
    /// The running launch's title, for the Guide menu. Main loop only.
    std::string runningTitle_;
    /// What the launch panel is up for, and the one install that runs. After steam_, so the
    /// install is stopped before the client it drives. Main loop only.
    PanelFlow panels_{shell_, sounds_, steam_,
                      PanelFlow::Hooks{[this] {
                                           cancelLaunch();
                                       },
                                       [this] {
                                           reloadCatalog();
                                       }}};
    /// Inside Gamescope, how the window draws over a running game. Null elsewhere, where the
    /// window is hidden while a game runs and shown only for the Guide menu.
    std::unique_ptr<session::GamescopeOverlay> overlay_;
    std::unique_ptr<session::GameKeys> gameKeys_;
    /// Set by the control channel, read by the loop.
    std::atomic<bool> closeRequested_{false};
    std::atomic<bool> reloadRequested_{false};
    /// The store sign-ins the control channel drives.
    StoreSignIn signIn_;

    /// Whether a game is running. The launch thread sets it; only the loop acts on it.
    std::atomic<bool> gameRunning_{false};
    /// A toast raised off-thread, taken by the loop.
    std::mutex toastMutex_;
    std::string pendingToast_;
    bool pendingToastError_{false};
    bool hasPendingToast_{false};
    /// Opens a store's sign-in page; after the toast it raises, so it is joined first.
    std::jthread signInOpener_;
    /// The launch's latest progress, raised off-thread, taken by the loop.
    std::mutex progressMutex_;
    std::optional<launch::LaunchProgress> pendingProgress_;

    /// Buttons queued by the control channel, drained by the loop.
    std::mutex injectedMutex_;
    std::vector<std::pair<gamepad::Button, input::Device>> injected_;
    /// Text typed over the channel, as a physical keyboard would.
    std::vector<std::string> typed_;

    /// The published state and the frame-request handshake. Written only by the
    /// main loop and read from the control channel's threads.
    mutable std::mutex stateMutex_;
    ShellSnapshot published_;
    std::condition_variable captureAnswered_;
    bool capturePending_{false};
    std::string captureResult_;
};

} // namespace opensu::app
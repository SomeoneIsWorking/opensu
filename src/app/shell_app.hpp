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

#include "artwork_delivery.hpp"
#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "audio/sound_player.hpp"
#include "audio_outputs.hpp"
#include "bluetooth_session.hpp"
#include "breadcrumb_trail.hpp"
#include "brightness_control.hpp"
#include "config/config.hpp"
#include "context_menu_controller.hpp"
#include "control_bridge.hpp"
#include "control_channel.hpp"
#include "controller_roster.hpp"
#include "details_controller.hpp"
#include "device/battery.hpp"
#include "devices_controller.hpp"
#include "game_screen.hpp"
#include "gamepad/direction_repeat.hpp"
#include "gamepad/pads.hpp"
#include "gamescope_windows.hpp"
#include "guide_menu_controller.hpp"
#include "host_services.hpp"
#include "input/keyboard_bindings.hpp"
#include "launch/handoff.hpp"
#include "layout_picker.hpp"
#include "library/catalog.hpp"
#include "library/catalog_loader.hpp"
#include "library/shelf.hpp"
#include "panel_flow.hpp"
#include "path_editor.hpp"
#include "pointer_router.hpp"
#include "preferences.hpp"
#include "quick_menu_controller.hpp"
#include "search_controller.hpp"
#include "session_install_controller.hpp"
#include "settings_controller.hpp"
#include "shortcut_editor.hpp"
#include "shortcut_router.hpp"
#include "sign_in.hpp"
#include "steam/client.hpp"
#include "ui/shell.hpp"
#include "volume_control.hpp"

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
    /// openSU is the login session (`--session`), not an app on a desktop.
    bool loginSession{false};
    /// Whether `run` starts the Steam client, when a Steam install exists.
    bool startSteam{true};
};

/// The running shell.
class ShellApp final : private PointerHost {
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
    void chooseLevel(int level) override;
    void contextMenu(const ui::PointerTarget& target) override;
    /// A click on a dock item: the same section change as L1 and R1.
    void clickSection(library::Section section);
    void actOn(gamepad::Button button);
    /// Moves home focus, with iiSU's Navigation sound when it moved.
    void moveFocus(ui::Direction direction);
    /// L1 and R1: moves `delta` sections along the dock and shows the section's shelf, with iiSU's
    /// domino cue sized by what the section shows at once.
    void cycleSection(int delta);
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
    /// The folders of the Steam libraries the installation holds now.
    [[nodiscard]] std::vector<std::filesystem::path> steamLibraries() const;
    /// Feeds a physical keyboard's text into a line entry (`PathEditor`,
    /// `SessionInstallController`), which takes typed text, Backspace, Enter and Escape.
    template <class Entry> void typeInto(Entry& entry, const input::TextInput& typed);
    /// Reads the keyboard as text while the search panel is open. `typed` was read before the
    /// frame's key bindings ran.
    void handleSearchText(const input::TextInput& typed);
    /// Opens a store's sign-in page in the browser, off the loop.
    void startSignIn(Store store);
    /// Whether a menu cannot open now: a launch, a chooser, the search or a typed field has the
    /// buttons.
    [[nodiscard]] bool menusBlocked();
    /// Ends the running game from a menu.
    void closeRunningGame();
    /// Guide: opens the Guide menu, or closes whichever menu is open.
    void toggleGuide();
    /// The quick menu action: opens the quick menu, or closes it.
    void toggleQuickMenu();
    /// Closes the details, Settings and Devices pages, which a Guide menu entry leaves.
    void closePages();
    /// Shows the window over a running game while a menu or page is up, and only then.
    void syncOverlay();
    /// Keeps Bluetooth read while something shows it and the Devices page current. Main loop only.
    void serviceDevices(std::chrono::steady_clock::time_point now);
    /// The output's size and refresh rate, for the Display tab.
    [[nodiscard]] static std::string outputLine();
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
    Settings settings_;
    /// What the player chose, read at start and saved when it changes.
    Preferences preferences_{settings::Store{config::read().configDir / "settings.json"},
                             [this](const std::string& why) {
                                 shell_.setToast(why, true);
                             }};
    /// The environment's configuration with what the player set in place of it: the roots the
    /// catalog and Steam read, fixed for the run.
    config::Config resolved_{settings::resolved(config::read(), preferences_.values())};
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
    /// What the control channel's threads and the loop hand each other.
    ControlBridge bridge_;
    ui::Shell shell_;
    /// Hands the fetched artwork and the stored sounds on.
    ArtworkDelivery delivery_{shell_, sounds_, artworkStore_, artworkFetcher_, [this] {
                                  bridge_.requestCatalogReload({});
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
                                                   },
                                                   [this] {
                                                       settingsScreen_.open();
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
    /// The volume shortcuts and the display that follows every change of the volume.
    VolumeControl volume_{VolumeControl::detect(config::read(), settings_.hidden),
                          VolumeControl::Hooks{[this](const audio::VolumeState& state) {
                                                   shell_.showVolume(
                                                       ui::VolumeLevel{state.percent, state.muted});
                                                   settingsScreen_.refresh();
                                                   devicesScreen_.refresh();
                                                   quickMenu_.refresh();
                                               },
                                               [this](const std::string& text, bool isError) {
                                                   shell_.setToast(text, isError);
                                               }}};
    /// Power, Bluetooth and the backlight; a hidden run reads them and changes nothing.
    HostServices host_{HostServices::detect(resolved_, settings_.hidden)};
    BluetoothSession bluetooth_{*host_.bluetooth};
    AudioOutputs outputs_{volume_.system()};
    BrightnessControl brightness_{host_.backlight.get()};
    /// Where keys, pad chords and the game-time watcher come out as the shell's buttons.
    ShortcutRouter router_{shell_.shortcuts(), volume_,
                           ShortcutRouter::Hooks{[this] {
                                                     bridge_.requestClose();
                                                 },
                                                 [this] {
                                                     toggleQuickMenu();
                                                 }}};
    /// Remaps a shortcut on the Settings screen.
    ShortcutEditor shortcutEditor_{shell_.shortcuts(), preferences_,
                                   [this](const std::string& text, bool isError) {
                                       shell_.setToast(text, isError);
                                   },
                                   [this] {
                                       settingsScreen_.refresh();
                                   }};
    /// Chooses the folders the Settings screen asks for.
    PathEditor paths_{shell_.settingsPanels().folders(), shell_.settingsPanels().entry(), sounds_,
                      [this](const std::string& text, bool isError) {
                          shell_.setToast(text, isError);
                      }};
    /// The Settings screen, reached from the options panel.
    SettingsController settingsScreen_{
        shell_.settingsPanels().page(),
        SettingsController::Services{paths_, shortcutEditor_, volume_, shell_.shortcuts()},
        sounds_,
        preferences_,
        config::read(),
        SettingsController::Hooks{[this] {
                                      applyLayout();
                                  },
                                  [this] {
                                      showShelf(0);
                                  },
                                  [this] {
                                      reloadCatalog();
                                  },
                                  [this] {
                                      return library::sourceChoices(games_, sources_);
                                  },
                                  [this] {
                                      return steamLibraries();
                                  },
                                  [this](const std::string& text, bool isError) {
                                      shell_.setToast(text, isError);
                                  }}};
    gamepad::Pads pads_{padsDirectory(settings_.hidden)};
    device::ControllerBatteries padBatteries_;
    /// The pads as the Devices page and the quick menu list them.
    ControllerRoster roster_{ControllerRoster::Sources{[this] {
                                                           return pads_.connected();
                                                       },
                                                       [this](const std::string& uniq) {
                                                           return padBatteries_.read(uniq);
                                                       }}};
    /// The Devices page, reached from the Guide menu and the quick menu.
    DevicesController devicesScreen_{
        shell_.devicesPanel().page(),
        DevicesController::Services{bluetooth_, roster_, outputs_, volume_, brightness_}, sounds_,
        preferences_,
        DevicesController::Hooks{[this] {
                                     applyLayout();
                                 },
                                 [this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 },
                                 [this] {
                                     devicesScreen_.close();
                                     settingsScreen_.openCategory("controls");
                                 },
                                 [] {
                                     return outputLine();
                                 }}};
    /// Installing session mode: its dialog, the password keyboard and the root step.
    SessionInstallController sessionInstall_{
        shell_.sessionPanels().dialog(), shell_.sessionPanels().password(), *host_.session, sounds_,
        [this](const std::string& text, bool isError) {
            shell_.setToast(text, isError);
        }};
    /// The menu Guide opens.
    GuideMenuController guideMenu_{
        shell_.guidePanels().guide(), *host_.power, *host_.session, sounds_,
        GuideMenuController::Hooks{[this] {
                                       return ui::GuideContext{
                                           shell_.inGame(), runningTitle_, settings_.loginSession,
                                           !settings_.loginSession && host_.session->installed()};
                                   },
                                   [this](library::Section section) {
                                       closePages();
                                       clickSection(section);
                                   },
                                   [this] {
                                       closePages();
                                       devicesScreen_.open();
                                   },
                                   [this] {
                                       closePages();
                                       settingsScreen_.open();
                                   },
                                   [this] {
                                       closeRunningGame();
                                   },
                                   [this] {
                                       bridge_.requestClose();
                                   },
                                   [this] {
                                       sessionInstall_.begin();
                                   },
                                   [this](const std::string& text, bool isError) {
                                       shell_.setToast(text, isError);
                                   }}};
    /// The quick menu: Guide + A, over the home screen or a running game.
    QuickMenuController quickMenu_{
        shell_.guidePanels().quick(),
        QuickMenuController::Services{volume_, outputs_, brightness_, bluetooth_, roster_}, sounds_,
        QuickMenuController::Hooks{[this] {
                                       return shell_.inGame();
                                   },
                                   [this] {
                                       closeRunningGame();
                                   },
                                   [this] {
                                       closePages();
                                       devicesScreen_.open();
                                   },
                                   [this](const std::string& text, bool isError) {
                                       shell_.setToast(text, isError);
                                   }}};
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
                                       },
                                       [this](library::Source store) {
                                           return preferences_.values().installFolders.effective(
                                               store);
                                       }}};
    /// How the window relates to a running game: the overlay inside Gamescope, hidden elsewhere.
    GameScreen gameScreen_{settings_.hidden};
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
};

} // namespace opensu::app
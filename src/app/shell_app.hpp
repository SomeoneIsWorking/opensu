// app — composition. Owns the catalog, the controller reader and the drawn
// shell, and is the only place the two halves meet.
#pragma once

#include <algorithm>
#include <array>
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

#include "artwork_delivery.hpp"
#include "artwork_fetcher.hpp"
#include "artwork_settings.hpp"
#include "artwork_store.hpp"
#include "audio/sdl_sound_player.hpp"
#include "audio_outputs.hpp"
#include "bluetooth_session.hpp"
#include "breadcrumb_trail.hpp"
#include "brightness_control.hpp"
#include "chord_settings.hpp"
#include "config/config.hpp"
#include "context_menu_controller.hpp"
#include "control_bridge.hpp"
#include "control_channel.hpp"
#include "controller_roster.hpp"
#include "details_controller.hpp"
#include "devices_controller.hpp"
#include "downloads_controller.hpp"
#include "emulator_downloads.hpp"
#include "emulator_settings.hpp"
#include "first_run_chooser.hpp"
#include "focus_follow.hpp"
#include "frame_generation.hpp"
#include "game_launch.hpp"
#include "game_screen.hpp"
#include "game_settings.hpp"
#include "game_settings_page.hpp"
#include "gamepad/direction_repeat.hpp"
#include "gamepad/pad_source.hpp"
#include "gamescope_windows.hpp"
#include "guide_menu_controller.hpp"
#include "host_services.hpp"
#include "input/keyboard_bindings.hpp"
#include "integration_settings.hpp"
#include "integration_switch.hpp"
#include "integrations_in_force.hpp"
#include "launch/handoff.hpp"
#include "launch/process_tree.hpp"
#include "launch_mailbox.hpp"
#include "layout_picker.hpp"
#include "library/catalog.hpp"
#include "library/catalog_loader.hpp"
#include "library/lossless_scaling.hpp"
#include "library/shelf.hpp"
#include "mapping_editor.hpp"
#include "pad_owner.hpp"
#include "panel_flow.hpp"
#include "path_editor.hpp"
#include "plugin_service.hpp"
#include "plugin_settings.hpp"
#include "pointer_router.hpp"
#include "preferences.hpp"
#include "quick_menu_controller.hpp"
#include "retro_session.hpp"
#include "search_controller.hpp"
#include "session_install_controller.hpp"
#include "settings_controller.hpp"
#include "shell_window.hpp"
#include "shortcut_editor.hpp"
#include "shortcut_router.hpp"
#include "sign_in.hpp"
#include "steam/client.hpp"
#include "steam_library_settings.hpp"
#include "steam_window_hiding.hpp"
#include "steamgriddb_covers.hpp"
#include "top_bar_clock.hpp"
#include "ui/shell.hpp"
#include "uninstall_prompt.hpp"
#include "update_request.hpp"
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
    /// `environment` outlives the app; it is what `config::read` returned at start.
    ShellApp(const Settings& settings, const config::Config& environment);

    /// Loads the library, then runs the window loop until it closes.
    int run();

    /// Renders one frame offscreen and writes it to `path`, behind a window that is never mapped.
    /// This is what makes the layout checkable from a test.
    bool renderToFile(const std::string& path, bool keyboardPrompts = false);

    /// The catalog as last read.
    [[nodiscard]] const std::vector<library::Game>& games() const noexcept {
        return games_;
    }

  private:
    /// Publishes state for the control channel, and serves any pending request.
    /// Main loop only.
    void publishSnapshot();
    void serviceControlRequests();
    void serviceRequests();

    /// Asks every store to list again; the listings reach the shell as they arrive.
    void reloadCatalog();
    /// Takes the listings that arrived and shows them. Main loop only.
    void serviceCatalog();
    /// Lists every store and waits for all of them; for a still render only.
    void loadCatalogNow();
    void applyCatalog(library::CatalogSnapshot snapshot);
    void handleEvents(const std::vector<gamepad::Event>& events);
    /// The daisy wheel that takes the left stick's directions now: the open keyboard the pad
    /// types on, when it shows the wheel; else nothing, and the stick moves focus as the d-pad
    /// does.
    [[nodiscard]] ui::OnScreenKeyboard* wheelTakingStick();
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
    ui::PointerTarget pointAt(std::optional<ui::Point> point) override;
    void focus(const ui::PointerTarget& target) override;
    void press(gamepad::Button button) override;
    void activateSection(library::Section section) override;
    void selectLauncher(library::Source source) override;
    /// Opens the Downloads page in place of any other page.
    void openDownloads() override;
    void activateCrumb(std::size_t index) override;
    void scroll(int steps) override;
    void chooseLevel(int level) override;
    void chooseTab(std::size_t index) override;
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
    /// Updates the shelf in place as the catalogue fills in, without replaying the entrance.
    void refreshShelf();
    /// The chrome the shelf's cards, title and status show, after either shelf update.
    void finishShelf(const settings::Settings& chosen);
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
    /// Closes the details, Settings, Devices and Downloads pages, which a Guide menu entry leaves.
    void closePages();
    /// Shows the window over a running game while a menu or page is up, and only then.
    void syncOverlay();
    /// Keeps Bluetooth read while something shows it and the Devices page current. Main loop only.
    void serviceDevices(std::chrono::steady_clock::time_point now);
    /// The output's size and refresh rate, for the Display tab.
    [[nodiscard]] std::string outputLine() const;
    /// Opens the window and attaches the shell to its device; false, logged, when it cannot.
    bool openWindow(const std::string& title, bool hidden);
    /// Gives the shell what its chrome shows now: prompts, launcher badges, title and trail.
    void syncChrome();
    /// The trail the top bar shows for where the shell is now.
    [[nodiscard]] ui::Trail currentTrail() const;
    /// Shows the running game under its video settings again after `game`'s layers changed, when
    /// it is that game on a built-in core.
    void applyVideo(const library::Game& game);
    /// The game whose window is in front: the running game, unless Gamescope's focus is on
    /// another window; null when none.
    [[nodiscard]] const library::Game* gameInFront() const;
    /// Runs ROM `game` on emulator `name` from now on, and keeps the pick.
    void chooseEmulator(const library::Game& game, const std::string& name);
    /// The game with `id` as the grid shows it now.
    [[nodiscard]] std::optional<library::Game> shownGame(const std::string& id) const;
    /// The game with `id` as its source lists it, its own cover in `artwork`; null when gone.
    [[nodiscard]] const library::Game* ownGame(const std::string& id) const;
    /// What each button does now, for the corner prompts: the same tests the actions make.
    [[nodiscard]] ui::HintContext hints() const;
    void pushCatalogToShell();
    /// Puts a change of the integrations into effect and shows it.
    void applyIntegrations();
    const config::Config& environment_;
    Settings settings_;
    /// What the player chose, read at start and saved when it changes.
    Preferences preferences_{settings::Store{environment_.configDir / settings::settingsFile},
                             [this](const std::string& why) {
                                 shell_.setToast(why, true);
                             }};
    /// The per-console, per-core and per-game setting layers, read at start and saved on change.
    GameSettings gameSettings_{
        settings::GameSettingsStore{environment_.configDir / settings::gameSettingsFile},
        environment_.configDir / settings::settingsFile, [this](const std::string& why) {
            shell_.setToast(why, true);
        }};
    /// Which integrations and which Steam mode are in force; before everything that asks.
    IntegrationsInForce integrations_{preferences_, settings_.loginSession};
    /// The environment's configuration with what the player set in place of it: the roots the
    /// catalog and Steam read, fixed for the run.
    config::Config resolved_{settings::resolved(environment_, preferences_.values())};
    /// The arcade cores' romset lists, fetched off the library scan; before the catalog that
    /// matches with them.
    library::roms::RomsetLists romsetLists_{resolved_.cacheDir};
    /// Frame generation for the game in front: the Gamescope fork's atoms, set from its layers.
    FrameGeneration frameGeneration_{
        gameSettings_, openFrameGenOutput(environment_.insideGamescope), [this] {
            return library::findLosslessDll(
                library::steam::Library::discover(resolved_.home, resolved_.steamRoots));
        }};
    /// Lists every store off the loop; the loop takes what has arrived.
    library::CatalogLoader catalogLoader_;
    /// Downloaded artwork, and the worker that fills it in.
    artwork::ArtworkStore artworkStore_{environment_.cacheDir / "artwork"};
    artwork::ArtworkFetcher artworkFetcher_{artworkStore_, artwork::RemoteSources{},
                                            library::roms::NameDb::under(environment_.cacheDir)};
    /// SteamGridDB's API, asked only with the player's key.
    artwork::SteamGridDb steamGridDb_{environment_.steamGridDbApi.empty()
                                          ? std::string{artwork::steamGridDbApi}
                                          : environment_.steamGridDbApi};
    /// The player's SteamGridDB key, handed to `steamGridDb_`.
    SteamGridDbAccount steamGridDbAccount_{SteamGridDbAccount::Parts{
        host::SecretStore{environment_.configDir / "secrets", environment_.keyring}, steamGridDb_}};
    /// iiSU's UI sounds, played from the loop's input handling.
    audio::SdlSoundPlayer sounds_;
    /// Opened by `run` or `renderToFile`; before the shell, whose textures live on its device.
    std::unique_ptr<ShellWindow> window_;
    /// What the control channel's threads and the loop hand each other.
    ControlBridge bridge_;
    ui::Shell shell_;
    TopBarClock clock_{shell_, environment_.clock24Hour};
    /// Hands the fetched artwork and the stored sounds on.
    /// Covers the player chose or SteamGridDB fitted, and "Choose artwork".
    SteamGridDbCovers covers_{
        shell_, sounds_, steamGridDb_,
        SteamGridDbCovers::Places{environment_.dataDir / "artwork" / "chosen",
                                  environment_.cacheDir / "artwork" / "fitted",
                                  environment_.cacheDir / "artwork" / "steamgriddb-thumbs"},
        SteamGridDbCovers::Hooks{[this](const std::string& id) {
                                     return ownGame(id);
                                 },
                                 [this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 }}};
    ArtworkDelivery delivery_{
        shell_, sounds_, artworkStore_, artworkFetcher_,
        ArtworkDelivery::Hooks{[this] {
                                   bridge_.requestCatalogReload({});
                               },
                               [this](const std::string& id, const std::filesystem::path& file) {
                                   covers_.coverArrived(id, file);
                               }}};
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
    /// The two presses of Uninstall on the details page and the tile menu.
    UninstallPrompt uninstallPrompt_{
        UninstallPrompt::Hooks{[this](const library::Game& game) {
                                   return panels_.installs().removal(game);
                               },
                               [this](const library::Game& game) {
                                   return panels_.installs().uninstall(game);
                               },
                               [this](const std::string& text, bool isError) {
                                   shell_.setToast(text, isError);
                               }}};
    /// Update on the details page and the tile menu.
    UpdateRequest updateRequest_{
        UpdateRequest::Hooks{[this](const library::Game& game) {
                                 return panels_.installs().update(game);
                             },
                             [this](const std::string& text, bool isError) {
                                 shell_.setToast(text, isError);
                             }}};
    ContextMenuController contextMenu_{
        shell_.contextMenu(),
        sounds_,
        preferences_,
        uninstallPrompt_,
        updateRequest_,
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
    /// Emulator downloads, updates and removals, off the loop.
    EmulatorDownloads emulatorDownloads_{
        library::roms::EmulatorSetup::standard(resolved_, settings_.hidden,
                                               integrations_.current().emulators),
        EmulatorDownloads::Hooks{[this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 },
                                 [this] {
                                     emulatorSettings_.rescan();
                                     reloadCatalog();
                                 },
                                 [this] {
                                     settingsScreen_.refresh();
                                     details_.refresh();
                                 }}};
    /// The Settings screen's Emulators category.
    EmulatorSettings emulatorSettings_{
        emulatorDownloads_, EmulatorSettings::Hooks{[this](const std::string& text, bool isError) {
            shell_.setToast(text, isError);
        }}};
    /// Settings › Emulators' pages for Global, each console and each core.
    LayerSettings layerSettings_{gameSettings_,
                                 [this]() -> const library::roms::Emulators& {
                                     return emulatorSettings_.found();
                                 },
                                 emulatorDownloads_.shaderPack(),
                                 LayerSettings::Hooks{[this] {
                                                          reloadCatalog();
                                                      },
                                                      [this] {
                                                          details_.refresh();
                                                      }}};
    /// The button mappings, and their page over the details page and Settings › Controls.
    MappingEditor mappings_{
        shell_.mappingPage(),
        settings::MappingStore{environment_.configDir / settings::mappingsFile}, sounds_,
        shell_.shortcuts(),
        MappingEditor::Hooks{[this](const std::string& text, bool isError) {
                                 shell_.setToast(text, isError);
                             },
                             [this] {
                                 settingsScreen_.refresh();
                             },
                             [this] {
                                 std::vector<MappingEditor::Emulator> emulators;
                                 for (const auto& entry : emulatorSettings_.catalog().entries()) {
                                     emulators.push_back({entry.id, entry.name});
                                 }
                                 return emulators;
                             }}};
    DetailsController details_{
        shell_.detailsPage(),
        sounds_,
        preferences_,
        gameSettings_,
        emulatorDownloads_,
        uninstallPrompt_,
        updateRequest_,
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
                                 },
                                 [this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 },
                                 [this](const library::Game& game) {
                                     mappings_.openGame(game);
                                 },
                                 [this](const library::Game& game) {
                                     gameSettingsPage_.open(game, nullptr);
                                 },
                                 [this] {
                                     return frameGeneration_.available();
                                 },
                                 [this](const library::Game& game) {
                                     covers_.choose(game.id);
                                 }}};
    /// The volume shortcuts and the display that follows every change of the volume.
    VolumeControl volume_{VolumeControl::detect(environment_, settings_.hidden),
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
    /// The running game's opensu-retro control channel, when it runs on one.
    RetroSession retro_{environment_.runtimeDir};
    /// Where keys, pad chords and the game-time watcher come out as the shell's buttons.
    ShortcutRouter router_{shell_.shortcuts(), shell_.chords(), volume_, retro_,
                           ShortcutRouter::Hooks{.quit =
                                                     [this] {
                                                         bridge_.requestClose();
                                                     },
                                                 .quickMenu =
                                                     [this] {
                                                         toggleQuickMenu();
                                                     },
                                                 .gameRunning =
                                                     [this] {
                                                         return shell_.inGame();
                                                     },
                                                 .closeGame =
                                                     [this] {
                                                         closeRunningGame();
                                                     },
                                                 .openDevices =
                                                     [this] {
                                                         closePages();
                                                         devicesScreen_.open();
                                                     },
                                                 .goHome =
                                                     [this] {
                                                         closePages();
                                                         clickSection(library::Section::Home);
                                                     },
                                                 .openDownloads =
                                                     [this] {
                                                         openDownloads();
                                                     },
                                                 .say =
                                                     [this](const std::string& text, bool isError) {
                                                         shell_.setToast(text, isError);
                                                     }}};
    /// Remaps a shortcut on the Settings screen; the pads keep the chords from the game.
    ShortcutEditor shortcutEditor_{shell_.shortcuts(), preferences_,
                                   [this](const std::string& text, bool isError) {
                                       shell_.setToast(text, isError);
                                   },
                                   [this] {
                                       settingsScreen_.refresh();
                                   },
                                   [this] {
                                       padOwner_.syncChords();
                                   }};
    /// The Settings screen's Guide chords category.
    ChordSettings chordSettings_{shortcutEditor_, shell_.shortcuts()};
    /// Chooses the folders the Settings screen asks for.
    PathEditor paths_{shell_.settingsPanels().folders(), shell_.settingsPanels().entry(), sounds_,
                      [this](const std::string& text, bool isError) {
                          shell_.setToast(text, isError);
                      }};
    /// The plugins, their play time and the Settings screen's Plugins category.
    PluginService plugins_{environment_, preferences_, integrations_.current().plugins,
                           PluginService::Hooks{[this](const std::string& text, bool isError) {
                                                    shell_.setToast(text, isError);
                                                },
                                                [this] {
                                                    plugins_.apply(games_);
                                                    refreshShelf();
                                                },
                                                [this] {
                                                    settingsScreen_.refresh();
                                                }}};
    PluginSettings pluginSettings_{plugins_, shell_.sessionPanels().dialog(), sounds_,
                                   environment_.home, [this] {
                                       settingsScreen_.refresh();
                                   }};
    /// Settings › Wine, and the runner and prefix a Windows launch gets.
    WineSettings wineSettings_{preferences_, resolved_, integrations_.current()};
    /// Settings › Integrations.
    IntegrationSettings integrationSettings_{integrations_, [this] {
                                                 applyIntegrations();
                                             }};
    /// Settings › Artwork and its key keyboard.
    ArtworkSettings artworkSettings_{
        steamGridDbAccount_, shell_.settingsPanels().entry(), sounds_,
        ArtworkSettings::Hooks{[this](const std::string& text, bool isError) {
                                   shell_.setToast(text, isError);
                               },
                               [this] {
                                   reloadCatalog();
                               },
                               [this] {
                                   settingsScreen_.refresh();
                               }}};
    /// The Settings screen, reached from the options panel.
    SettingsController settingsScreen_{
        shell_.settingsPanels().page(),
        SettingsController::Services{paths_, shortcutEditor_, volume_, shell_.shortcuts(),
                                     emulatorSettings_, layerSettings_, steamLibrarySettings_,
                                     pluginSettings_, chordSettings_, mappings_, wineSettings_,
                                     integrationSettings_, artworkSettings_, integrations_},
        sounds_,
        preferences_,
        environment_,
        SettingsController::Hooks{[this] {
                                      applyLayout();
                                  },
                                  [this] {
                                      refreshShelf();
                                  },
                                  [this] {
                                      reloadCatalog();
                                  },
                                  [this] {
                                      return library::sourceChoices(games_, sources_);
                                  },
                                  [this] {
                                      return Installs::ownFolders(
                                          resolved_, steamLibrarySettings_.defaultLibrary(),
                                          integrations_.current());
                                  },
                                  [this](const std::string& text, bool isError) {
                                      shell_.setToast(text, isError);
                                  }}};
    std::unique_ptr<gamepad::PadSource> pads_{
        gamepad::openPadSource(PadOwner::padsFor(environment_.dataDir, settings_.hidden))};
    /// Who has the pads: the shell, the running launch with its mapping, or another window.
    /// Main loop only.
    PadOwner padOwner_{*pads_, shell_.chords()};
    device::ControllerBatteries padBatteries_;
    /// The pads as the Devices page and the quick menu list them.
    ControllerRoster roster_{ControllerRoster::Sources{[this] {
                                                           return pads_->connected();
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
                                 [this] {
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
                                           shell_.inGame(), launches_.game().title,
                                           settings_.loginSession,
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
                                       openDownloads();
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
        QuickMenuController::Services{volume_, outputs_, brightness_, bluetooth_, roster_, retro_},
        sounds_,
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
                                   },
                                   [this] {
                                       gameSettingsPage_.open(launches_.game(), [this] {
                                           quickMenu_.reopenOnGameSettings();
                                       });
                                   },
                                   [this] {
                                       return frameGeneration_.available();
                                   }}};
    /// The game settings tabs, from the quick menu over a running game or over the details page.
    GameSettingsPage gameSettingsPage_{
        shell_.guidePanels().tabs(),
        gameSettings_,
        emulatorDownloads_,
        frameGeneration_,
        retro_,
        sounds_,
        GameSettingsPage::Hooks{[this](const library::Game& game) {
                                    details_.refresh();
                                    applyVideo(game);
                                    frameGeneration_.changed();
                                },
                                [this](const std::string& text, bool isError) {
                                    shell_.setToast(text, isError);
                                }}};
    /// After shell_ and the host it drives.
    PointerRouter pointer_{shell_.inputDevice(), *this};
    /// Held directions, from a pad or the keyboard, repeat on one schedule.
    gamepad::DirectionRepeat repeat_;

    /// Created after construction, because it drives this shell.
    std::unique_ptr<ControlChannel> control_;

    std::vector<library::Game> games_;
    /// How many of `games_` are installed, counted when the catalog arrives.
    std::size_t installedCount_{0};
    /// Each store's state from the last catalog read.
    std::vector<library::SourceStatus> sources_;
    /// Home or the open folder. Main loop only.
    library::ShelfBrowser browser_;
    /// Started in run(), before the loop, and shut down with the app.
    steam::Client steam_;
    /// The Settings screen's Steam libraries category; after the client it asks.
    SteamLibrarySettings steamLibrarySettings_{
        SteamLibrarySettings::Sources{steam_.libraries(),
                                      [this] {
                                          return steam_.state() == launch::SteamState::Ready;
                                      },
                                      [this] {
                                          return library::steam::Library::discover(
                                                     resolved_.home, resolved_.steamRoots)
                                              .libraryFolders();
                                      }},
        paths_, resolved_.home,
        SteamLibrarySettings::Hooks{[this](const std::string& text, bool isError) {
                                        shell_.setToast(text, isError);
                                    },
                                    [this] {
                                        settingsScreen_.refresh();
                                    }}};
    /// Inside Gamescope, tells the handoff when a game shows a window. Null elsewhere.
    std::unique_ptr<session::GamescopeWindows> gameWindows_;
    /// Inside Gamescope, keeps openSU's own Steam's windows out of sight. Null elsewhere.
    std::unique_ptr<session::SteamWindowHiding> steamWindowHiding_;
    launch::Handoff handoff_;
    /// What the launch panel is up for, and the one install that runs. After steam_, so the
    /// install is stopped before the client it drives. Main loop only.
    PanelFlow panels_{shell_, sounds_, steam_, environment_.dataDir,
                      PanelFlow::Hooks{[this] {
                                           cancelLaunch();
                                       },
                                       [this] {
                                           reloadCatalog();
                                       },
                                       [this](library::Source store) {
                                           return preferences_.values().installFolders.effective(
                                               store);
                                       },
                                       [this](const library::Game& game) {
                                           return launches_.runs(game);
                                       }}};
    /// The Downloads page: Steam's queue, the store install, the emulator download and the romset
    /// lists. After the sources it reads.
    DownloadsController downloadsScreen_{
        shell_.downloadsPanel().page(),
        DownloadsController::Sources{
            .installs = panels_.installs(),
            .emulators = emulatorDownloads_,
            .romsetLists = romsetLists_,
            .steam = steam_.downloadControl(),
            .steamReady =
                [this] {
                    return steam_.state() == launch::SteamState::Ready;
                },
            .steamQueue =
                [this] {
                    return steam_.downloads();
                },
            .titles =
                [this](library::Source store, const std::string& id) {
                    const auto game =
                        std::ranges::find_if(games_, [store, &id](const library::Game& one) {
                            return one.source == store && one.sourceId == id;
                        });
                    return game != games_.end() ? game->title : std::string{};
                }},
        sounds_,
        DownloadsController::Hooks{.say =
                                       [this](const std::string& text, bool isError) {
                                           shell_.setToast(text, isError);
                                       },
                                   .romsetListsEnded =
                                       [this] {
                                           reloadCatalog();
                                       }}};
    /// How the window relates to a running game: the overlay inside Gamescope, hidden elsewhere.
    GameScreen gameScreen_{settings_.hidden, [this](bool blocked) {
                               padOwner_.setBlocked(blocked);
                           }};
    /// Gamescope's focus, inside it; after steam_, so a Big Picture close in flight ends first.
    FocusFollow focusFollow_{FocusFollow::Hooks{.gamePids =
                                                    [this] {
                                                        return launch::ProcessTree::treesMatching(
                                                            launches_.game().processHint);
                                                    },
                                                .closeBigPicture =
                                                    [this] {
                                                        steam_.bigPicture().close();
                                                    },
                                                .takeFocus =
                                                    [this] {
                                                        gameScreen_.takeFocus();
                                                    }}};
    /// Puts the integrations in force into effect; after everything it drives.
    IntegrationSwitch integrationSwitch_{
        integrations_,
        IntegrationSwitch::Targets{steam_, catalogLoader_, plugins_, emulatorDownloads_, padOwner_,
                                   *pads_, focusFollow_},
        IntegrationSwitch::Hooks{
            .catalog =
                [this](const library::CatalogStores& stores) {
                    return library::makeCatalog(resolved_, settings_.hidden, romsetLists_, stores);
                },
            .steamInstalled =
                [this] {
                    return !library::steam::Library::discover(resolved_.home, resolved_.steamRoots)
                                .roots()
                                .empty();
                },
            .emulatorsChanged =
                [this] {
                    emulatorSettings_.rescan();
                },
            .hideSteamWindows =
                [this](bool hides) {
                    if (steamWindowHiding_) {
                        steamWindowHiding_->hide(hides);
                    }
                }}};
    /// The first-run card that asks which integrations to use.
    FirstRunChooser firstRun_{shell_.sessionPanels().integrations(), integrations_, sounds_,
                              [this] {
                                  applyIntegrations();
                              }};
    /// The store sign-ins the control channel drives.
    StoreSignIn signIn_;

    /// What the launch and sign-in threads hand the loop.
    LaunchMailbox mailbox_;
    /// The game the shell runs; after the handoff, the mailbox and the runner session its thread
    /// uses, so destroying it closes the game and joins that thread before they go.
    GameLaunch launches_{handoff_, mailbox_, retro_};
    /// Opens a store's sign-in page; after the mailbox it posts to, so it is joined first.
    std::jthread signInOpener_;
};

} // namespace opensu::app
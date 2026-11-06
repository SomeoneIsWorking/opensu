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
#include "console_themes.hpp"
#include "context_menu_controller.hpp"
#include "control_bridge.hpp"
#include "control_channel.hpp"
#include "controller_roster.hpp"
#include "details_controller.hpp"
#include "devices_controller.hpp"
#include "downloads_controller.hpp"
#include "emulator_downloads.hpp"
#include "emulator_settings.hpp"
#include "focus_follow.hpp"
#include "frame_timing.hpp"
#include "game_backdrop.hpp"
#include "game_launch.hpp"
#include "game_match_controller.hpp"
#include "game_presentation.hpp"
#include "game_screen.hpp"
#include "game_settings.hpp"
#include "gamepad/direction_repeat.hpp"
#include "gamepad/pad_source.hpp"
#include "giveaways/giveaway_board.hpp"
#include "giveaways_controller.hpp"
#include "guide_menu_controller.hpp"
#include "home_screen.hpp"
#include "host_services.hpp"
#include "input/keyboard_bindings.hpp"
#include "integration_settings.hpp"
#include "integration_switch.hpp"
#include "integrations_in_force.hpp"
#include "interface_sound_settings.hpp"
#include "interface_sounds.hpp"
#include "keyboard_pad.hpp"
#include "lan/remote_library.hpp"
#include "lan_netplay.hpp"
#include "lan_profiles.hpp"
#include "lan_roms.hpp"
#include "lan_service.hpp"
#include "lan_settings.hpp"
#include "launch/handoff.hpp"
#include "launch/process_tree.hpp"
#include "launch_mailbox.hpp"
#include "layered_editor.hpp"
#include "layered_settings.hpp"
#include "library/catalog.hpp"
#include "library/catalog_loader.hpp"
#include "library/game_saves.hpp"
#include "library/lossless_scaling.hpp"
#include "library/shelf.hpp"
#include "library_additions.hpp"
#include "mapping_editor.hpp"
#include "netplay_run.hpp"
#include "night_light.hpp"
#include "offer_claim.hpp"
#include "online_play.hpp"
#include "own_games_controller.hpp"
#include "pad_owner.hpp"
#include "panel_flow.hpp"
#include "path_editor.hpp"
#include "player_files.hpp"
#include "player_profiles.hpp"
#include "plugin_service.hpp"
#include "plugin_settings.hpp"
#include "polkit_prompt_controller.hpp"
#include "preferences.hpp"
#include "profile_settings.hpp"
#include "remote_input.hpp"
#include "remote_picture.hpp"
#include "remote_play.hpp"
#include "remote_touch.hpp"
#include "retro/resume_file.hpp"
#include "retro_session.hpp"
#include "save_copy_controller.hpp"
#include "save_sharing.hpp"
#include "search_controller.hpp"
#include "session_install_controller.hpp"
#include "settings_controller.hpp"
#include "shared_saves.hpp"
#include "shell_session.hpp"
#include "shell_window.hpp"
#include "shortcut_editor.hpp"
#include "shortcut_router.hpp"
#include "sign_in.hpp"
#include "steam/client.hpp"
#include "steam_library_settings.hpp"
#include "steamgriddb_covers.hpp"
#include "together_questions.hpp"
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
    /// Where other openSUs on the LAN reach this one while LAN is on. Zero asks for a free port.
    std::uint16_t lanPort{0};
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
    /// Whether the UI sounds open the audio device; a hidden run only with `--sound`.
    bool playSounds{true};
};

/// The running shell.
class ShellApp final {
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
    /// The player's ROM folders changed: the catalog reads them from now on.
    void libraryFoldersChanged();
    /// Keeps `folder` with the player's ROM folders, which the catalog reads from now on.
    void addRomFolder(const std::filesystem::path& folder);
    /// Takes the listings that arrived and shows them. Main loop only.
    void serviceCatalog();
    /// Lists every store and waits for all of them; for a still render only.
    void loadCatalogNow();
    /// The catalog of `stores`: the stores' providers, and the paired openSUs' games with LAN on.
    [[nodiscard]] library::Catalog catalogOf(const library::CatalogStores& stores);
    void applyCatalog(library::CatalogSnapshot snapshot);
    /// Shows the chosen console art and asks the fetcher for what the store still wants.
    void requestArtwork();
    void handleEvents(const std::vector<gamepad::Event>& events);
    /// Maps held keys onto the buttons they stand in for, so the keyboard reaches
    /// the same actions a controller does rather than a parallel set.
    void handleKeyboard();
    /// Keyboard shortcuts pressed while a game has the keyboard (Shift+Tab is Guide).
    void handleGameKeys();
    /// Pad events, noting the pad as the device in use.
    void handlePads();
    /// The window's pointer this frame. Main loop only.
    void handlePointer();
    /// Notes the pointer as the device in use and hands one frame of it to the navigator, which
    /// is the only way the pointer reaches the shell.
    void pointer(const PointerFrame& frame);
    /// A click on a launcher's badge in the top bar: the store's tile in Library.
    void selectLauncher(library::Source source);
    /// Opens the Downloads page in place of any other page.
    void openDownloads();
    /// A click on a level of the breadcrumb trail: go back to it.
    void activateCrumb(std::size_t index);
    /// A dock section chosen, by a click or L1 and R1: moves along the dock to it.
    void clickSection(library::Section section);
    /// Moves along the dock to `section` from wherever the dock stands.
    void moveToSection(library::Section section);
    /// A Guide place: `section`'s shelf, with the pages closed and out of any search or folder. The
    /// menu is still on the stack, so `clickSection`'s check would refuse.
    void goToSection(library::Section section);
    void actOn(gamepad::Button button);
    /// B on Home: out of the search or the folder, else ESC opens the Guide menu.
    void backOnHome();
    /// Home, at the bottom of the navigator, and the chrome's hooks.
    void buildHome();
    /// L1 and R1: moves `delta` sections along the dock and shows the section's shelf, with iiSU's
    /// domino cue sized by what the section shows at once.
    void cycleSection(int delta);
    /// Launches `game` as `profile` with `environment` added to its own, or offers to install it;
    /// that profile's saves are synced with the openSUs sharing them first, and another openSU's
    /// game is asked of it. False when it was refused.
    bool launch(const library::Game& game, std::vector<std::string> environment,
                const std::string& profile);
    /// Starts `game` now as `profile` with `saves` as its save folder, `extra` added to its
    /// environment; false when it was refused.
    bool startLaunch(const library::Game& game, std::vector<std::string> extra,
                     const std::string& profile, const std::filesystem::path& saves);
    /// Starts a netplay session's `game` as the active profile with its saves in `saves`, never
    /// synced with the profile's; false when it was refused.
    bool launchSession(const library::Game& game, const std::filesystem::path& saves);
    /// The core options `game` would run with on the link core.
    [[nodiscard]] std::map<std::string, std::string>
    netplayOptionsOf(const library::Game& game) const;
    /// The BIOS files `game`'s built-in core boots from here.
    [[nodiscard]] std::vector<std::filesystem::path> netplayBiosOf(const library::Game& game) const;
    /// The folder `profile`'s games save in.
    [[nodiscard]] std::filesystem::path profileRunnerSaves(const std::string& profile) const;
    /// Abandons a launch whose game has not appeared yet, such as one waiting on a Steam update.
    void cancelLaunch();
    /// Where the active profile's resume state of ROM `game` is kept.
    [[nodiscard]] std::filesystem::path resumeFileOf(const library::Game& game) const;
    /// The header of the resume state Save and close left of `game` on the built-in runner.
    [[nodiscard]] std::optional<retro::ResumeHeader> resumeOf(const library::Game& game) const;
    /// Throws that state away for Start fresh: empty when it is gone, else why not.
    [[nodiscard]] std::string startFresh(const library::Game& game) const;
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
    [[nodiscard]] std::vector<SearchResult> searchResults() const;
    /// Focuses tile `index` and presses A on it.
    void openTile(std::size_t index);
    /// A on the focused tile: opens its folder, the giveaways or its details.
    void pressTile();
    /// Hides or shows `game` everywhere, and keeps it.
    void setHidden(const library::Game& game, bool hidden);
    /// Plays as the profile `id` from now on: its saves are the ones games load, Copy saves copies
    /// and save sync shares. Refused while a game runs.
    void switchProfile(const std::string& id);
    /// Asks which profile plays, the active one focused, once there is more than one.
    void askProfile();
    /// Whether `item` is on Home; nothing for a tile that cannot be put there.
    [[nodiscard]] std::optional<bool> onHome(const library::ShelfItem& item) const;
    /// Puts `item` on Home or takes it off, and keeps it.
    void setOnHome(const library::ShelfItem& item, bool on);
    /// Notes that `game` was launched now, for the recently played sort.
    void recordLaunch(const library::Game& game);
    /// Feeds a physical keyboard's text into the focused text field: typed text, Backspace as
    /// Erase, Enter as Submit, Escape as Back, and Up and Down to move off the field.
    void typeIntoField(const input::TextInput& typed);
    /// Opens a store's sign-in page in the browser, off the loop.
    void startSignIn(Store store);
    /// Whether a menu cannot open now: a launch, a chooser, the search or a typed field has the
    /// buttons.
    [[nodiscard]] bool menusBlocked();
    /// Ends the running game from a menu.
    void closeRunningGame();
    /// Shows the newest picture of a remote game and hands it to the shell to draw.
    void showGamePicture();
    /// Whether a remote game has the player's controls: it streams and no menu is up.
    [[nodiscard]] bool remoteHasControls();
    /// Sends the player's controls to the remote game, or lets go of them while a menu is up.
    void sendRemoteInput();
    /// Where the remote picture is drawn in the window.
    [[nodiscard]] display::Placed remotePlacement() const;
    /// Posts an injected pointer event to the window's queue, as the mouse would make it.
    void postPointer(const PointerInjection& injected);
    /// Caps remote play's link for a test run; refused outside a hidden one.
    void capRemoteLink(std::uint64_t bitsPerSecond);
    /// Guide, or Guide + A on Volume: opens the Guide menu, or closes it.
    void toggleGuide(GuideMenuController::Landing landing);
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
    /// Opens `game`'s settings in the layered editor, as a page or a panel over the running game.
    void openGameSettings(const library::Game& game, EditorLook look, EditorRows more = {});
    /// The game whose window is in front: the running game, unless Gamescope's focus is on
    /// another window; null when none.
    [[nodiscard]] const library::Game* gameInFront() const;
    /// Runs ROM `game` on emulator `name` from now on, and keeps the pick.
    /// The store copies with a launch button each on `game`'s page here.
    [[nodiscard]] std::vector<library::Game> copiesOffered(const library::Game& game) const;
    /// Keeps `key`, titled `title`, as what `game` is for its artwork (Match a different game).
    void keepMatch(const library::Game& game, const std::string& key, const std::string& title);
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
    /// Where the player's files are kept: in place, or a hidden run's copies.
    PlayerFiles files_{PlayerFiles::forRun(environment_, settings_.hidden)};
    /// What the player chose, read at start and saved when it changes.
    Preferences preferences_{settings::Store{files_.settingsFile()},
                             [this](const std::string& why) {
                                 shell_.setToast(why, true);
                             }};
    /// The per-console, per-core and per-game setting layers, read at start and saved on change.
    GameSettings gameSettings_{settings::GameSettingsStore{files_.gameSettingsFile()},
                               files_.settingsFile(), [this](const std::string& why) {
                                   shell_.setToast(why, true);
                               }};
    /// Which integrations and which Steam mode are in force; before everything that asks.
    IntegrationsInForce integrations_{preferences_, settings_.loginSession};
    /// The environment's configuration with what the player set in place of it: the roots the
    /// catalog and Steam read, fixed for the run but for the library folders
    /// (`libraryFoldersChanged`); the runner's saves where `files_` keeps them.
    config::Config resolved_{
        files_.savingIn(settings::resolved(environment_, preferences_.values()))};
    /// The player profiles, opened before anything reads saves, the saves from before profiles
    /// moved into the first one's.
    PlayerProfiles profiles_{files_, resolved_.savesDir};
    /// The arcade cores' romset lists, fetched off the library scan; before the catalog that
    /// matches with them.
    library::roms::RomsetLists romsetLists_{resolved_.cacheDir};
    /// Frame generation for the game in front and the running game's resolution and upscaling:
    /// Gamescope's atoms, set from their layers.
    GamePresentation presentation_{
        gameSettings_, openPresentationOutput(environment_.insideGamescope), [this] {
            return library::findLosslessDll(
                library::steam::Library::discover(resolved_.home, resolved_.steamRoots));
        }};
    /// The paired openSUs' games, which `lan_` fills and the catalog lists; before the catalog.
    std::shared_ptr<lan::RemoteLibrary> remoteGames_{std::make_shared<lan::RemoteLibrary>()};
    /// Lists every store off the loop; the loop takes what has arrived.
    library::CatalogLoader catalogLoader_;
    /// Downloaded artwork, and the worker that fills it in.
    artwork::ArtworkStore artworkStore_{environment_.cacheDir / "artwork"};
    /// The console art themes, shipped and the player's own.
    artwork::ConsoleThemes consoleThemes_{environment_.assetsDir / "consoles",
                                          environment_.dataDir / "themes"};
    /// The theme the home shows, kept while its id and the iiSU file placed stay the same.
    std::shared_ptr<const artwork::ConsoleTheme> consoleTheme_;
    std::string consoleThemeId_;
    std::shared_ptr<const artwork::IisuPack> consoleThemeIisu_;
    artwork::ArtworkFetcher artworkFetcher_{artworkStore_, artwork::RemoteSources::of(environment_),
                                            library::roms::NameDb::under(environment_.cacheDir)};
    /// SteamGridDB's API, asked only with the player's key.
    artwork::SteamGridDb steamGridDb_{environment_.steamGridDbApi.empty()
                                          ? std::string{artwork::steamGridDbApi}
                                          : environment_.steamGridDbApi};
    /// The UI sounds, played from the loop's input handling.
    audio::SdlSoundPlayer sounds_;
    /// Whose sounds `sounds_` holds, as the preferences choose.
    InterfaceSounds interfaceSounds_{sounds_, environment_.assetsDir, consoleThemes_.iisu()};
    /// Opened by `run` or `renderToFile`; before the shell, whose textures live on its device.
    std::unique_ptr<ShellWindow> window_;
    /// What the control channel's threads and the loop hand each other.
    ControlBridge bridge_;
    ui::Shell shell_;
    /// The remote game's picture the shell draws, on the window's device once it is open.
    std::unique_ptr<RemotePicture> remotePicture_;
    std::unique_ptr<GameBackdrop> gameBackdrop_;
    /// The player's controls while a remote game has them.
    RemoteInput remoteInput_;
    std::unique_ptr<RemoteTouch> remoteTouch_;
    TopBarClock clock_{shell_, environment_.clock24Hour,
                       device::BatteryReader{environment_.batteryStandIn}};
    FrameTiming frameTiming_;
    /// The on-screen keyboard the pad types into text fields with.
    KeyboardPad keyboardPad_{shell_.keyboard(), shell_.screens().navigator(), sounds_};
    /// Match a different game.
    GameMatchController gameMatch_{
        GameMatchController::Parts{shell_.screens().navigator(), shell_.screens().chrome(),
                                   shell_.screens().images(), keyboardPad_, sounds_},
        GameMatchController::Places{environment_.cacheDir / "artwork" / "match",
                                    artwork::RemoteSources::of(environment_)},
        artworkStore_,
        GameMatchController::Hooks{
            [this](const std::string& text, bool isError) {
                shell_.setToast(text, isError);
            },
            [this](const library::Game& game, const std::string& key, const std::string& title) {
                keepMatch(game, key, title);
            }}};
    /// Hands the fetched artwork and the stored sounds on.
    /// Covers the player chose or SteamGridDB fitted, and "Choose artwork".
    SteamGridDbCovers covers_{
        shell_,
        sounds_,
        steamGridDb_,
        artworkStore_,
        SteamGridDbCovers::Places{files_.chosenCoversDir(),
                                  environment_.cacheDir / "artwork" / "fitted",
                                  environment_.cacheDir / "artwork" / "steamgriddb-thumbs"},
        SteamGridDbCovers::Hooks{[this](const std::string& id) {
                                     return ownGame(id);
                                 },
                                 [this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 },
                                 [this] {
                                     settingsScreen_.openCategory(
                                         std::string{ArtworkSettings::categoryId});
                                     artworkSettings_.connect();
                                 },
                                 [this](const std::string& /*id*/) {
                                     details_.refresh();
                                 },
                                 [this](const std::string& id) {
                                     if (const library::Game* game = ownGame(id)) {
                                         gameMatch_.open(*game);
                                     }
                                 }}};
    ArtworkDelivery delivery_{
        shell_, artworkFetcher_,
        ArtworkDelivery::Hooks{[this] {
                                   bridge_.requestCatalogReload({});
                               },
                               [this](const std::string& id, const std::filesystem::path& file) {
                                   covers_.coverArrived(id, file);
                               },
                               [this](const std::string& id) {
                                   covers_.coverMissing(id);
                               }}};
    /// After the preferences they keep and the shelf they re-show.
    ShelfOptions shelfOptions_{preferences_, ShelfOptions::Hooks{[this] {
                                                                     applyLayout();
                                                                 },
                                                                 [this] {
                                                                     refreshShelf();
                                                                 },
                                                                 [this] {
                                                                     return library::sourceChoices(
                                                                         games_, sources_);
                                                                 }}};
    SearchController search_{SearchController::Parts{.navigator = shell_.screens().navigator(),
                                                     .chrome = shell_.screens().chrome(),
                                                     .keyboard = keyboardPad_,
                                                     .sounds = sounds_,
                                                     .preferences = preferences_},
                             SearchController::Hooks{[this](std::size_t focus) {
                                                         showShelf(focus);
                                                     },
                                                     [this] {
                                                         return searchResults();
                                                     },
                                                     [this](std::size_t index) {
                                                         openTile(index);
                                                     }}};
    /// Uninstall on the details page and the tile menu.
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
        shell_.screens().navigator(), sounds_,
        ContextMenuController::Services{preferences_, uninstallPrompt_, updateRequest_},
        ContextMenuController::Hooks{
            [this] {
                return shell_.grid().focusedItem();
            },
            [this] {
                if (const library::Game* game = shell_.grid().focusedGame()) {
                    static_cast<void>(launch(*game, {}, profiles_.book().active().id));
                }
            },
            [this] {
                details_.open();
            },
            [this](const library::Game& game, bool hidden) {
                setHidden(game, hidden);
            },
            [this](const library::ShelfItem& item) {
                return onHome(item);
            },
            [this](const library::ShelfItem& item, bool on) {
                setOnHome(item, on);
            },
            [this](const library::Folder& folder) {
                openFolder(folder);
            },
            [this] {
                reloadCatalog();
                shell_.setToast("library refreshed");
            },
            [this](library::Source source) {
                startSignIn(source == library::Source::Gog ? Store::Gog : Store::Epic);
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
    /// What the layered editor edits, from Settings, a game's details or the Guide menu.
    LayeredSettings layered_{
        LayeredSettings::Sources{gameSettings_,
                                 [this]() -> const library::roms::Emulators& {
                                     return emulatorSettings_.found();
                                 },
                                 emulatorDownloads_.shaderPack(), presentation_, retro_,
                                 CoreOptionStore{environment_.cacheDir / "core-options"}},
        LayeredSettings::Hooks{[this](const std::string& id) -> std::optional<library::Game> {
                                   if (const library::Game* game = ownGame(id)) {
                                       return *game;
                                   }
                                   return std::nullopt;
                               },
                               [this]() -> const library::Game* {
                                   return shell_.inGame() ? &launches_.game() : nullptr;
                               },
                               [this] {
                                   reloadCatalog();
                               },
                               [this] {
                                   details_.refresh();
                                   settingsScreen_.refresh();
                               },
                               [this](const std::string& text, bool isError) {
                                   shell_.setToast(text, isError);
                               }}};
    /// Settings › Emulators' rows for Global, each console and each core.
    LayerSettings layerSettings_{gameSettings_,
                                 [this]() -> const library::roms::Emulators& {
                                     return emulatorSettings_.found();
                                 },
                                 layered_};
    /// The button mappings, and their page over the details page and Settings › Controls.
    MappingEditor mappings_{
        shell_.mappingPage(), settings::MappingStore{files_.mappingsFile()}, sounds_,
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
    /// Copy saves over the details page; its backups under the state dir.
    SaveCopyController saveCopy_{
        shell_.screens().navigator(),
        shell_.screens().chrome(),
        sounds_,
        emulatorDownloads_,
        SaveCopyController::Places{profiles_, files_.stateDir()},
        SaveCopyController::Hooks{[this](const std::string& text, bool isError) {
                                      shell_.setToast(text, isError);
                                  },
                                  [this] {
                                      details_.refresh();
                                  }}};
    DetailsController details_{
        DetailsController::Place{shell_.screens().navigator(), shell_.screens().chrome(),
                                 shell_.screens().images()},
        sounds_,
        DetailsController::Services{preferences_, gameSettings_, emulatorDownloads_,
                                    uninstallPrompt_, updateRequest_},
        DetailsController::Hooks{
            [this] {
                return shell_.grid().focusedGame();
            },
            [this](const std::string& id) {
                return shownGame(id);
            },
            [this](const library::Game& game) {
                static_cast<void>(launch(game, {}, profiles_.book().active().id));
            },
            [this](const library::Game& game, bool hidden) {
                setHidden(game, hidden);
            },
            [this](const library::Game& game) {
                return onHome(game).value_or(false);
            },
            [this](const library::Game& game) {
                return std::ranges::find(game.playingAs, profiles_.book().active().id) !=
                       game.playingAs.end();
            },
            [this](const library::Game& game, bool on) {
                setOnHome(game, on);
            },
            [this](const std::string& text, bool isError) {
                shell_.setToast(text, isError);
            },
            [this](const library::Game& game) {
                mappings_.openGame(game);
            },
            [this](const library::Game& game, std::function<std::vector<ui::SettingsRow>()> rows) {
                openGameSettings(game, EditorLook::Page, EditorRows{std::move(rows), &details_});
            },
            [this](const library::Game& game) -> std::optional<std::size_t> {
                if (!layered_.offers(game)) {
                    return std::nullopt;
                }
                return layered_.kept(game);
            },
            [this](const library::Game& game) {
                covers_.choose(game.id);
            },
            [this](const library::Game& game) {
                ownGames_.rename(game);
            },
            [this](const library::Game& game) {
                ownGames_.changeFile(game);
            },
            [this](const library::Game& game) {
                ownGames_.remove(game);
            },
            [this](const std::string& id) {
                return shell_.grid().artworkDownloading(id);
            },
            [this](const library::Game& game) {
                gameMatch_.open(game);
            },
            [this](const library::Game& game) {
                saveCopy_.read(game);
                saveSharing_.look(game);
            },
            [this](const library::Game& game) {
                return saveCopy_.offer(game);
            },
            [this](const library::Game& game) {
                saveCopy_.open(game);
            },
            [this](const library::Game& game) {
                return sharedSaves_.view(game);
            },
            [this](const library::Game& game) {
                sharedSaves_.act(game);
            },
            [this](const library::Game& game) {
                return resumeOf(game);
            },
            [this](const library::Game& game) {
                return startFresh(game);
            },
            [this] {
                return lanNetplay_.partners();
            },
            [this](const library::Game& game, const std::string& peerId) {
                lanNetplay_.play(game, peerId);
            },
            [this](const library::Game& game) {
                onlinePlay_.host(game);
            },
            [this](const library::Game& game) {
                return copiesOffered(game);
            }}};
    /// The volume shortcuts and the display that follows every change of the volume.
    VolumeControl volume_{VolumeControl::detect(environment_, settings_.hidden),
                          VolumeControl::Hooks{[this](const audio::VolumeState& state) {
                                                   shell_.showVolume(
                                                       ui::VolumeLevel{state.percent, state.muted});
                                               },
                                               [this](const std::string& text, bool isError) {
                                                   shell_.setToast(text, isError);
                                               }}};
    /// Power, Bluetooth and the backlight; a hidden run reads them and changes nothing.
    HostServices host_{HostServices::detect(resolved_, settings_.hidden)};
    /// Null where the machine has no Bluetooth service.
    std::unique_ptr<BluetoothSession> bluetooth_{
        host_.bluetooth ? std::make_unique<BluetoothSession>(*host_.bluetooth) : nullptr};
    AudioOutputs outputs_{volume_.system()};
    BrightnessControl brightness_{host_.backlight.get()};
    /// The night light the player chose, shown by openSU's Gamescope when openSU is the login
    /// session; on a desktop the desktop's own warms the nested Gamescope.
    NightLight nightLight_{
        openDesktopNight(environment_.xdgConfigHome),
        openNightLightOutput(settings_.loginSession, environment_.insideGamescope)};
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
                                                         toggleGuide(
                                                             GuideMenuController::Landing::Volume);
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
    PathEditor paths_{shell_.screens().navigator(), shell_.screens().chrome(), sounds_};
    /// The player's own games: Add a game, and their rows on the details page.
    OwnGamesController ownGames_{
        OwnGamesController::Parts{shell_.screens().navigator(), paths_, keyboardPad_, sounds_},
        OwnGamesController::Places::of(resolved_, files_.ownGamesFile()),
        OwnGamesController::Hooks{[this](const std::string& text, bool isError) {
                                      shell_.setToast(text, isError);
                                  },
                                  [this] {
                                      reloadCatalog();
                                  }}};
    /// Remote play with the paired openSUs, both ways; before `lan_`, whose listener offers to it.
    RemotePlay remotePlay_{
        RemotePlay::Screen{.gamescopeSocket = environment_.gamescopeWaylandDisplay.empty()
                                                  ? std::filesystem::path{}
                                                  : environment_.runtimeDir.parent_path() /
                                                        environment_.gamescopeWaylandDisplay,
                           .x11Display = environment_.x11Display},
        RemotePlay::Hooks{
            .say =
                [this](const std::string& text, bool isError) {
                    shell_.setToast(text, isError);
                },
            .find = [this](const std::string& id) -> std::optional<library::Game> {
                const auto found = std::ranges::find_if(games_, [&id](const library::Game& game) {
                    return game.id == id && game.installed &&
                           game.source != library::Source::Remote;
                });
                return found == games_.end() ? std::nullopt : std::optional{*found};
            },
            .launch =
                [this](const library::Game& game, std::vector<std::string> environment,
                       const std::string& profile) {
                    return launch(game, std::move(environment), profile);
                },
            .close =
                [this] {
                    closeRunningGame();
                },
            .ask =
                [this](const std::string& hostId, const std::string& gameId,
                       std::function<void()> waiting, LanService::PlayAnswer answer) {
                    return lan_.askToPlay(hostId, gameId, profiles_.book().active().id,
                                          std::move(waiting), std::move(answer));
                },
            .speaker =
                [this] {
                    return sounds_.live(stream::audioRate, stream::audioChannels);
                },
            .invite =
                [this](const std::string& peerId, const lan::wire::Invitation& game,
                       std::function<void(std::string)> refused) {
                    return lan_.askPeers(
                        [peerId = peerId, game = game, refused = std::move(refused)](
                            const lan::LanNode& node, const std::stop_token& stop) {
                            std::string error;
                            if (!node.invite(stop, peerId, game, error) && !stop.stop_requested()) {
                                refused(std::move(error));
                            }
                        });
                },
            .join =
                [this](const std::string& hostId, const std::string& gameId) {
                    const auto found = std::ranges::find_if(games_, [&](const library::Game& game) {
                        return game.source == library::Source::Remote && game.hostId == hostId &&
                               game.sourceId == gameId;
                    });
                    return found != games_.end() && launch(*found, {}, {});
                },
            .name =
                [this] {
                    return lan_.name();
                },
            .lendPad =
                [this] {
                    return pads_->lendPad();
                },
            .returnPad =
                [this] {
                    pads_->returnPad();
                },
            .seats =
                [this](std::string id, std::vector<std::string> profiles) {
                    lan_.playing(std::move(id), std::move(profiles));
                }}};
    /// The ROM folders as LAN import writes into them; the import's thread shares it.
    std::shared_ptr<const library::roms::RomTree> romTree_{library::roms::openRomTree()};
    /// Saves shared with the share Settings › Saves names and the paired openSUs.
    SaveSharing saveSharing_{
        save_sync::SaveSync::Setup{.versionsFile = files_.saveVersionsFile(),
                                   .backups = files_.stateDir(),
                                   .staging = files_.stateDir() / "saves-incoming",
                                   .runnerSaves = resolved_.savesDir,
                                   .inUse = {},
                                   .changed = {},
                                   .seatOf = {}},
        save_sync::EpicCloud::Setup{.legendary = "legendary",
                                    .recordFile = files_.stateDir() / save_sync::epicSavesFile,
                                    .backups = files_.stateDir(),
                                    .changed = {}},
        files_.secrets(environment_.keyring), profiles_.book(), [this] {
            details_.refresh();
        }};
    /// This openSU on the LAN while LAN is on: pairing, and the paired openSUs' games.
    LanService lan_{
        LanService::Places{.bookFile = files_.lanBookFile(),
                           .secrets = files_.secrets(environment_.keyring),
                           .coverCache = resolved_.cacheDir / "lan",
                           .port = settings_.lanPort,
                           .saves = saveSharing_.engine(),
                           .profiles = profiles_.book(),
                           .roms = LanRoms::importSetup(files_.stateDir() / "lan-roms", romTree_)},
        preferences_,
        shell_.screens().navigator(),
        sounds_,
        remoteGames_,
        integrations_.current().lan,
        LanService::Hooks{.say =
                              [this](const std::string& text, bool isError) {
                                  shell_.setToast(text, isError);
                              },
                          .remoteChanged =
                              [this] {
                                  reloadCatalog();
                              },
                          .peersChanged =
                              [this] {
                                  settingsScreen_.refresh();
                              },
                          .romsChanged =
                              [this] {
                                  lanRoms_.progressed();
                              },
                          .play =
                              [this](const lan::Peer& peer, const std::string& game,
                                     const std::string& profile, bool linked,
                                     const lan::StreamKeys& keys) {
                                  return remotePlay_.offer(peer, game, profile, linked, keys);
                              },
                          .playStatus =
                              [this](const lan::Peer& peer, const std::string& game) {
                                  return remotePlay_.playStatus(peer, game);
                              },
                          .invited =
                              [this](const lan::Peer& peer, const lan::wire::Invitation& game) {
                                  return remotePlay_.invited(peer, game);
                              }}};
    /// What play together asks the player here.
    TogetherQuestions togetherQuestions_{shell_.screens().navigator(), sounds_, remotePlay_};
    /// The details page's Saves fact and row for saves shared with paired openSUs.
    SharedSaves sharedSaves_{saveSharing_, shell_.screens().navigator(), sounds_};
    /// Settings › Saves.
    SaveSettings saveSettings_{SaveSettings::Services{.sharing = saveSharing_,
                                                      .preferences = preferences_,
                                                      .paths = paths_,
                                                      .navigator = shell_.screens().navigator(),
                                                      .keyboard = keyboardPad_,
                                                      .sounds = sounds_},
                               files_.secrets(environment_.keyring), [this] {
                                   settingsScreen_.refresh();
                               }};
    /// Settings › Profiles.
    ProfileSettings profileSettings_{
        profiles_.book(), shell_.screens().navigator(), keyboardPad_, sounds_,
        ProfileSettings::Hooks{.changed =
                                   [this] {
                                       settingsScreen_.refresh();
                                   },
                               .say =
                                   [this](const std::string& text, bool error) {
                                       shell_.setToast(text, error);
                                   },
                               .play =
                                   [this](const std::string& id) {
                                       switchProfile(id);
                                   },
                               .peerName =
                                   [this](const std::string& peerId) {
                                       return lan_.peerName(peerId);
                                   },
                               .unlink =
                                   [this](const std::string& profileId, const std::string& peerId) {
                                       lanProfiles_.unlink(profileId, peerId);
                                   }}};
    /// Settings › LAN.
    LanSettings lanSettings_{
        lan_, preferences_, shell_.screens().navigator(), keyboardPad_, sounds_, [this] {
            settingsScreen_.refresh();
        }};
    /// ROMs offered to paired openSUs, and those of theirs imported here.
    LanRoms lanRoms_{lan_, paths_, romTree_, resolved_.home,
                     LanRoms::Hooks{.say =
                                        [this](const std::string& text, bool isError) {
                                            shell_.setToast(text, isError);
                                        },
                                    .romFolders =
                                        [this] {
                                            return library::romRootsOf(resolved_);
                                        },
                                    .addRomFolder =
                                        [this](const std::filesystem::path& folder) {
                                            addRomFolder(folder);
                                        },
                                    .bios =
                                        [this](std::string_view system) {
                                            return emulatorSettings_.biosOf(system);
                                        },
                                    .biosFolder =
                                        [this] {
                                            return emulatorSettings_.biosFolder();
                                        },
                                    .reloadCatalog =
                                        [this] {
                                            reloadCatalog();
                                        },
                                    .refresh =
                                        [this] {
                                            settingsScreen_.refresh();
                                        }}};
    /// Profiles copied to and from paired openSUs, and their links.
    LanProfiles lanProfiles_{lan_,
                             profiles_.book(),
                             saveSharing_.engine(),
                             shell_.screens().navigator(),
                             sounds_,
                             LanProfiles::Hooks{.say =
                                                    [this](const std::string& text, bool isError) {
                                                        shell_.setToast(text, isError);
                                                    },
                                                .refresh =
                                                    [this] {
                                                        settingsScreen_.refresh();
                                                    }}};
    /// A netplay session on this machine, however it was agreed.
    NetplayRun netplayRun_{
        NetplayRun::Places{.sessionSaves = files_.stateDir() / "netplay" / "saves",
                           .sessionFile = files_.stateDir() / "netplay" / "session.json"},
        NetplayRun::Hooks{
            .say =
                [this](const std::string& text, bool isError) {
                    shell_.setToast(text, isError);
                },
            .profileSaves =
                [this] {
                    return profileRunnerSaves(profiles_.book().active().id);
                },
            .launch =
                [this](const library::Game& game, const std::filesystem::path& saves) {
                    return launchSession(game, saves);
                },
            .coreOptions =
                [this](const library::Game& game) {
                    return netplayOptionsOf(game);
                },
            .biosFolder =
                [this] {
                    return emulatorSettings_.biosFolder();
                },
            .coreBios =
                [this](const library::Game& game) {
                    return netplayBiosOf(game);
                }}};
    /// Playing a ROM linked with a paired openSU.
    LanNetplay lanNetplay_{
        lan_, shell_.screens().navigator(), sounds_, netplayRun_,
        LanNetplay::Hooks{.say =
                              [this](const std::string& text, bool isError) {
                                  shell_.setToast(text, isError);
                              },
                          .games = [this]() -> const std::vector<library::Game>& {
                              return games_;
                          },
                          .sessionPort =
                              [this] {
                                  return retro_.netplayPort();
                              }}};
    /// Playing a ROM with a friend over the internet, by code.
    OnlinePlay onlinePlay_{
        shell_.screens().navigator(),
        sounds_,
        netplayRun_,
        keyboardPad_,
        preferences_,
        OnlinePlay::Hooks{.say =
                              [this](const std::string& text, bool isError) {
                                  shell_.setToast(text, isError);
                              },
                          .games = [this]() -> const std::vector<library::Game>& {
                              return games_;
                          },
                          .copy =
                              [this](const std::string& text) {
                                  window_->setClipboard(text);
                              }}};
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
    PluginSettings pluginSettings_{plugins_, shell_.screens().navigator(), sounds_,
                                   environment_.home, [this] {
                                       settingsScreen_.refresh();
                                   }};
    /// GE-Proton and umu-launcher kept current under the data dir, off the loop; read only in a
    /// hidden run.
    WineRunners wineRunners_{
        resolved_.dataDir, resolved_.geProtonRelease, settings_.hidden,
        WineRunners::Hooks{.say =
                               [this](const std::string& text, bool isError) {
                                   shell_.setToast(text, isError);
                               },
                           .changed =
                               [this] {
                                   wineSettings_.rescan();
                                   settingsScreen_.refresh();
                               },
                           .refresh =
                               [this] {
                                   settingsScreen_.refresh();
                               },
                           .wanted =
                               [this] {
                                   return library::isReady(sources_, library::Source::Gog) ||
                                          library::isReady(sources_, library::Source::Epic);
                               },
                           .keepCurrent =
                               [this] {
                                   return preferences_.values().keepGeProtonCurrent;
                               },
                           .inUse =
                               [this] {
                                   return wineSettings_.inUse();
                               },
                           .gameRunning =
                               [this] {
                                   return launches_.running();
                               }}};
    /// Settings › Wine, and the runner and prefix a Windows launch gets.
    WineSettings wineSettings_{
        preferences_, resolved_, integrations_.current(), wineRunners_,
        WineSettings::Hooks{.say =
                                [this](const std::string& text, bool isError) {
                                    shell_.setToast(text, isError);
                                },
                            .games =
                                [this] {
                                    return games_;
                                },
                            .gameRunning =
                                [this] {
                                    return launches_.running();
                                }}};
    /// Settings › Integrations.
    IntegrationSettings integrationSettings_{integrations_, [this] {
                                                 applyIntegrations();
                                             }};
    /// Settings › Artwork and its key keyboard.
    ArtworkSettings artworkSettings_{
        steamGridDbAccount_, ArtworkSettings::Entry{shell_.screens().navigator(), keyboardPad_},
        ArtworkSettings::Services{preferences_, consoleThemes_, sounds_},
        ArtworkSettings::Hooks{[this](const std::string& text, bool isError) {
                                   shell_.setToast(text, isError);
                               },
                               [this] {
                                   reloadCatalog();
                               },
                               [this] {
                                   settingsScreen_.refresh();
                               },
                               [this] {
                                   requestArtwork();
                               }}};
    /// Settings › Audio › Interface sounds.
    InterfaceSoundSettings interfaceSoundSettings_{preferences_, consoleThemes_.iisu(), [this] {
                                                       applyLayout();
                                                       settingsScreen_.refresh();
                                                   }};
    /// The Settings screen, reached from the options panel.
    SettingsController settingsScreen_{
        shell_.screens().navigator(),
        shell_.screens().chrome(),
        SettingsController::Services{paths_,
                                     shortcutEditor_,
                                     shell_.shortcuts(),
                                     emulatorSettings_,
                                     layerSettings_,
                                     layered_,
                                     steamLibrarySettings_,
                                     pluginSettings_,
                                     chordSettings_,
                                     mappings_,
                                     wineSettings_,
                                     integrationSettings_,
                                     artworkSettings_,
                                     interfaceSoundSettings_,
                                     lanSettings_,
                                     lanRoms_,
                                     lanProfiles_,
                                     onlinePlay_,
                                     saveSettings_,
                                     profileSettings_,
                                     shelfOptions_,
                                     integrations_},
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
                                      return Installs::ownFolders(
                                          resolved_, steamLibrarySettings_.defaultLibrary(),
                                          integrations_.current());
                                  },
                                  [this](const std::string& text, bool isError) {
                                      shell_.setToast(text, isError);
                                  },
                                  [this] {
                                      libraryFoldersChanged();
                                  },
                                  [this] {
                                      return guideMenu_.isOpen();
                                  }}};
    /// Library's add tiles: Add a game, Add a game folder, Add a ROM folder, the integrations not
    /// set up.
    LibraryAdditions libraryAdditions_{
        integrations_,
        LibraryAdditions::Hooks{.addGame =
                                    [this] {
                                        ownGames_.openAdd();
                                    },
                                .addGameFolder =
                                    [this] {
                                        settingsScreen_.addGameFolder();
                                    },
                                .addRomFolder =
                                    [this] {
                                        settingsScreen_.addRomFolder();
                                    },
                                .importFromLan =
                                    [this] {
                                        settingsScreen_.openLanImport();
                                    },
                                .setUp =
                                    [this](const std::string& row) {
                                        settingsScreen_.openAt(
                                            std::string{IntegrationSettings::categoryId}, row);
                                    }}};
    std::unique_ptr<gamepad::PadSource> pads_{
        gamepad::openPadSource(PadOwner::padsFor(environment_.dataDir, settings_.hidden))};
    /// Who has the pads: the shell, the running launch with its mapping, or another window.
    /// Main loop only.
    PadOwner padOwner_{*pads_, shell_.chords()};
    /// The pads as the Devices page lists them.
    ControllerRoster roster_{ControllerRoster::Sources{[this] {
                                                           return pads_->connected();
                                                       },
                                                       [this](const gamepad::PadInfo& pad) {
                                                           return pads_->battery(pad);
                                                       }}};
    /// The Devices page, reached from the Guide menu.
    DevicesController devicesScreen_{
        shell_.screens().navigator(), shell_.screens().chrome(),
        DevicesController::Services{bluetooth_.get(), roster_, outputs_}, sounds_,
        DevicesController::Hooks{[this](const std::string& text, bool isError) {
                                     shell_.setToast(text, isError);
                                 },
                                 [this] {
                                     devicesScreen_.close();
                                     settingsScreen_.openCategory("controls");
                                 },
                                 [this] {
                                     return outputLine();
                                 }}};
    /// Installing session mode: its question and the root step through polkit. Null where there
    /// is no session mode.
    std::unique_ptr<SessionInstallController> sessionInstall_{
        host_.session
            ? std::make_unique<SessionInstallController>(
                  shell_.screens().navigator(), *host_.session, host_.polkit.get(), sounds_,
                  [this](const std::string& text, bool isError) {
                      shell_.setToast(text, isError);
                  })
            : nullptr};
    /// The password polkit asks openSU's own agent for. Null without the agent.
    std::unique_ptr<PolkitPromptController> polkitPrompt_{
        host_.polkit ? std::make_unique<PolkitPromptController>(
                           shell_.screens().navigator(), keyboardPad_, sounds_, *host_.polkit)
                     : nullptr};
    /// The menu Guide opens, over the home screen or a running game.
    GuideMenuController guideMenu_{
        shell_.screens().navigator(),
        GuideMenuController::Services{host_.power.get(), host_.session.get(), volume_, brightness_,
                                      retro_, preferences_, shelfOptions_, profiles_.book()},
        sounds_,
        GuideMenuController::Hooks{
            .context =
                [this] {
                    const bool inGame = shell_.inGame();
                    const bool remote = remotePlay_.streaming();
                    const bool here = inGame && !remote;
                    return GuideContext{
                        .inGame = inGame,
                        .remote = remote,
                        .joined = remote && remotePlay_.joined(),
                        .title = remote ? remotePlay_.game().title : launches_.game().title,
                        .gameSettings =
                            inGame && !remote && !layered_.sectionsOf(launches_.game()).empty(),
                        .loginSession = settings_.loginSession,
                        .sessionInstalled =
                            !settings_.loginSession && host_.session && host_.session->installed(),
                        .takesGuests = here && lan_.node() != nullptr && remotePlay_.takesGuests(),
                        .invitable = here ? remotePlay_.notGuests(lan_.announced())
                                          : std::vector<lan::Peer>{},
                        .guests = here ? remotePlay_.guests() : std::vector<lan::Peer>{},
                        .closesWithGuest = here && remotePlay_.closesWithGuest(),
                        .page = !inGame && shell_.screens().navigator().atRoot()
                                    ? std::optional{browser_.section()}
                                    : std::nullopt};
                },
            .showSection =
                [this](library::Section section) {
                    goToSection(section);
                },
            .devices = {.isOpen =
                            [this] {
                                return devicesScreen_.isOpen();
                            },
                        .open =
                            [this] {
                                devicesScreen_.open();
                            },
                        .close =
                            [this] {
                                devicesScreen_.close();
                            }},
            .downloads = {.isOpen =
                              [this] {
                                  return downloadsScreen_.isOpen();
                              },
                          .open =
                              [this] {
                                  downloadsScreen_.open();
                              },
                          .close =
                              [this] {
                                  downloadsScreen_.close();
                              }},
            .settings = {.isOpen =
                             [this] {
                                 return settingsScreen_.isOpen();
                             },
                         .open =
                             [this] {
                                 settingsScreen_.open();
                             },
                         .close =
                             [this] {
                                 settingsScreen_.close();
                             }},
            .openSearch =
                [this] {
                    search_.open();
                },
            .closeGame =
                [this] {
                    if (remotePlay_.streaming()) {
                        remotePlay_.end();
                    } else {
                        closeRunningGame();
                    }
                },
            .openGameSettings =
                [this] {
                    openGameSettings(launches_.game(), EditorLook::Panel);
                },
            .applyLayout =
                [this] {
                    applyLayout();
                },
            .quit =
                [this] {
                    bridge_.requestClose();
                },
            .installSession =
                [this] {
                    if (sessionInstall_) {
                        sessionInstall_->begin();
                    }
                },
            .say =
                [this](const std::string& text, bool isError) {
                    shell_.setToast(text, isError);
                },
            .playAs =
                [this](const std::string& id) {
                    switchProfile(id);
                },
            .invite =
                [this](const lan::Peer& peer) {
                    remotePlay_.invite(peer);
                },
            .kick =
                [this](const std::string& peerId) {
                    remotePlay_.kick(peerId);
                }}};
    /// Whether the pointer drove the shell last frame, so leaving is said once.
    bool pointerIn_{false};
    /// Whether the primary button or a finger is down.
    bool pointerHeld_{false};
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
    /// The player's SteamGridDB key, handed to `steamGridDb_`; after the Steam client its sign-in
    /// starts and waits on, so its worker ends first.
    SteamGridDbAccount steamGridDbAccount_{SteamGridDbAccount::Parts{
        .secrets = secrets::SecretStore{environment_.configDir / secrets::secretsFolder,
                                        environment_.keyring},
        .source = steamGridDb_,
        .steam = [this] {
            return integrationSwitch_.steamInUse();
        }}};
    /// The stores' free-to-keep offers, read in the background and kept in the cache.
    giveaways::GiveawayBoard giveawayBoard_{resolved_.cacheDir};
    /// Claim on a giveaway; after the Steam client whose Big Picture it may show the page in.
    OfferClaim offerClaim_{OfferClaim::Parts{.steam = [this] {
        return integrationSwitch_.steamInUse();
    }}};
    /// The Giveaways screens, opened from Library's tile.
    GiveawaysController giveaways_{GiveawaysController::Parts{
        .navigator = shell_.screens().navigator(),
        .chrome = shell_.screens().chrome(),
        .images = shell_.screens().images(),
        .board = giveawayBoard_,
        .claim = offerClaim_,
        .games = [this]() -> const std::vector<library::Game>& {
            return games_;
        },
        .turnedOn =
            [this] {
                std::vector<library::Source> on;
                for (const library::Source store :
                     {library::Source::Steam, library::Source::Gog, library::Source::Epic}) {
                    if (integrations_.current().lists(store)) {
                        on.push_back(store);
                    }
                }
                return on;
            },
        .say =
            [this](std::string text, bool error) {
                shell_.setToast(std::move(text), error);
            }}};
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
    /// The session the shell runs in (Gamescope's windows and focus); null outside one.
    std::unique_ptr<ShellSession> session_;
    launch::Handoff handoff_;
    /// What the launch panel is up for, and the one install that runs. After steam_, so the
    /// install is stopped before the client it drives. Main loop only.
    PanelFlow panels_{
        shell_.screens().navigator(), sounds_, steam_, environment_.dataDir,
        PanelFlow::Hooks{.cancelLaunch =
                             [this] {
                                 cancelLaunch();
                             },
                         .reloadCatalog =
                             [this] {
                                 reloadCatalog();
                             },
                         .installFolder =
                             [this](library::Source store) {
                                 return preferences_.values().installFolders.effective(store);
                             },
                         .playing =
                             [this](const library::Game& game) {
                                 return launches_.runs(game);
                             },
                         .say =
                             [this](const std::string& text, bool isError) {
                                 shell_.setToast(text, isError);
                             },
                         .gameRunning =
                             [this] {
                                 return shell_.inGame();
                             }}};
    /// The Downloads page: Steam's queue, the store install, the emulator download and the romset
    /// lists. After the sources it reads.
    DownloadsController downloadsScreen_{
        shell_.screens().navigator(), shell_.screens().chrome(),
        DownloadsController::Sources{
            .installs = panels_.installs(),
            .emulators = emulatorDownloads_,
            .runners = wineRunners_,
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
        IntegrationSwitch::Targets{steam_, catalogLoader_, plugins_, lan_, emulatorDownloads_,
                                   padOwner_, *pads_, focusFollow_},
        IntegrationSwitch::Hooks{
            .catalog =
                [this](const library::CatalogStores& stores) {
                    return catalogOf(stores);
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
                    if (session_) {
                        session_->hideSteamWindows(hides);
                    }
                },
            .steamWindowsWanted =
                [this] {
                    return steamGridDbAccount_.signingIn() || offerClaim_.inSteam();
                }}};
    /// The store sign-ins the control channel drives.
    StoreSignIn signIn_;

    /// What the launch and sign-in threads hand the loop.
    LaunchMailbox mailbox_;
    /// The game the shell runs; after the handoff, the mailbox and the runner session its thread
    /// uses, so destroying it closes the game and joins that thread before they go.
    GameLaunch launches_{handoff_, mailbox_, retro_};
    /// The profile the game `launches_` started last plays as: the active one, or a remote guest's.
    std::string playingAs_;
    /// Opens a store's sign-in page; after the mailbox it posts to, so it is joined first.
    std::jthread signInOpener_;
};

} // namespace opensu::app
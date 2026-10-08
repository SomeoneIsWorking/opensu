// app — composition. Owns the catalog, the controller reader and the drawn
// shell, and is the only place the two halves meet.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "raylib.h"

#include "artwork_fetcher.hpp"
#include "artwork_store.hpp"
#include "config/config.hpp"
#include "control_channel.hpp"
#include "device/battery.hpp"
#include "game_keys.hpp"
#include "gamepad/direction_repeat.hpp"
#include "gamepad/pads.hpp"
#include "gamescope_overlay.hpp"
#include "gamescope_windows.hpp"
#include "install_job.hpp"
#include "launch/handoff.hpp"
#include "library/catalog.hpp"
#include "library/shelf.hpp"
#include "sign_in.hpp"
#include "steam/client.hpp"
#include "ui/shell.hpp"

namespace iideck::app {

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
};

/// The running shell.
class ShellApp final : public ControlTarget {
  public:
    explicit ShellApp(Settings settings);

    /// Loads the library, then runs the window loop until it closes.
    int run();

    /// Renders one frame offscreen and writes it to `path`, without opening a
    /// window. This is what makes the layout checkable from a test.
    bool renderToFile(const std::string& path);

    /// The catalog as last read.
    [[nodiscard]] const std::vector<library::Game>& games() const noexcept {
        return games_;
    }

    // ControlTarget. Each is callable from another thread, and each hands work to
    // the main loop, because the OpenGL context and the shell's state belong to
    // it and to no other thread.
    [[nodiscard]] ShellSnapshot snapshot() const override;
    void inject(gamepad::Button button) override;
    [[nodiscard]] bool captureFrame(std::string& png) override;
    void requestClose() override;
    void requestCatalogReload(std::string toast) override;

  private:
    /// Flips an image in place, for the render texture's bottom-up origin.
    static void flipVertical(Image& image);

    /// Renders the current state offscreen and encodes it as PNG. Main loop
    /// only, with a live GL context.
    [[nodiscard]] bool renderFrameToPng(std::string& png);

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

    void reloadCatalog();
    void handleEvents(const std::vector<gamepad::Event>& events);
    /// Maps held keys onto the buttons they stand in for, so the keyboard reaches
    /// the same actions a controller does rather than a parallel set.
    void handleKeyboard();
    /// Keyboard shortcuts pressed while a game has the keyboard (Shift+Tab is Guide).
    void handleGameKeys();
    void actOn(gamepad::Button button);
    void launchFocused();
    /// Abandons a launch whose game has not appeared yet, such as one waiting on a Steam update.
    void cancelLaunch();
    /// Buttons while the launch panel is up: it takes them all. Main loop only.
    void actOnPanel(gamepad::Button button);
    /// Asks whether to install the focused game, which is not installed.
    void offerInstall(const library::Game& game);
    /// Shows the install job's news on the panel and the catalog. Main loop only.
    void serviceInstall();
    /// Asks the player about the install's licence agreements on the panel.
    void presentEula();
    /// Shows the shelf the browser is on, focusing `focus`.
    void showShelf(std::size_t focus);
    /// Shows artwork the fetcher has downloaded. Main loop only.
    void serviceArtwork();
    /// Opens the focused console on its ROMs.
    void openConsole(const library::Console& console);
    /// Buttons while a game runs: Guide opens and closes the menu over it, which takes the
    /// rest. Main loop only.
    void actInGame(gamepad::Button button);
    /// Opens or closes the Guide menu and shows or hides the window drawing it.
    void setGameMenuOpen(bool open);
    void showDetails();
    /// Re-reads the clock and the battery and schedules the next minute boundary.
    void refreshClock();
    void pushCatalogToShell();

    Settings settings_;
    library::Catalog catalog_;
    /// Downloaded artwork, and the worker that fills it in.
    artwork::ArtworkStore artworkStore_{config::read().cacheDir / "artwork"};
    artwork::ArtworkFetcher artworkFetcher_{artworkStore_, artwork::RemoteSources{}};
    device::BatteryReader battery_;
    /// When the clock next changes, on the minute boundary.
    std::chrono::steady_clock::time_point nextClockTick_{};
    ui::Shell shell_;
    gamepad::Pads pads_;
    /// Held directions, from a pad or the keyboard, repeat on one schedule.
    gamepad::DirectionRepeat repeat_;
    /// The pads are held for the running launch. Main loop only.
    bool padsHeld_{false};

    /// Created after construction, because it drives this shell.
    std::unique_ptr<ControlChannel> control_;

    std::vector<library::Game> games_;
    /// Each store's state from the last catalog read.
    std::vector<library::SourceStatus> sources_;
    /// Home or the open console. Main loop only.
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
    /// What the launch panel is up for. Main loop only.
    enum class PanelUse { None, Launch, OfferInstall, Install, Eula };
    PanelUse panelUse_{PanelUse::None};
    /// A licence question that arrived while a launch held the panel.
    bool eulaWaiting_{false};
    /// The game the panel offers to install. Main loop only.
    std::optional<library::Game> offered_;
    /// After steam_, so it is stopped before the client it drives.
    InstallJob install_{steam_};
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
    /// The launch's latest progress, raised off-thread, taken by the loop.
    std::mutex progressMutex_;
    std::optional<launch::LaunchProgress> pendingProgress_;

    /// Buttons queued by the control channel, drained by the loop.
    std::mutex injectedMutex_;
    std::vector<gamepad::Button> injected_;

    /// The published state and the frame-request handshake. Written only by the
    /// main loop and read from the control channel's threads.
    mutable std::mutex stateMutex_;
    ShellSnapshot published_;
    std::condition_variable captureAnswered_;
    bool capturePending_{false};
    std::string captureResult_;
};

} // namespace iideck::app
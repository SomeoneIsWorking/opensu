// app — composition. Owns the catalog, the controller reader and the drawn
// shell, and is the only place the two halves meet.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "raylib.h"

#include "control_channel.hpp"
#include "gamepad/reader.hpp"
#include "launch/handoff.hpp"
#include "library/catalog.hpp"
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
    void requestWindowVisible(bool visible);
    void requestToast(std::string text, bool isError);

    void reloadCatalog();
    void handleEvents(const std::vector<gamepad::Event>& events);
    void actOn(gamepad::Button button);
    void launchFocused();
    void showDetails();
    void refreshClock();
    void pushCatalogToShell();

    Settings settings_;
    library::Catalog catalog_;
    ui::Shell shell_;
    gamepad::Reader pad_;

    /// Created after construction, because it drives this shell.
    std::unique_ptr<ControlChannel> control_;

    std::vector<library::Game> games_;
    /// Guards the handoff thread, which touches the window.
    std::mutex launchMutex_;
    bool launchRunning_{false};
    /// Set by the control channel, read by the loop.
    std::atomic<bool> closeRequested_{false};

    /// Whether the window should be up. The launch thread clears it while a game
    /// runs; only the loop acts on it.
    std::atomic<bool> windowVisible_{true};
    /// What the window is actually doing, so the flag is only acted on once.
    bool windowShown_{true};
    /// A toast raised off-thread, taken by the loop.
    std::mutex toastMutex_;
    std::string pendingToast_;
    bool pendingToastError_{false};
    bool hasPendingToast_{false};

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
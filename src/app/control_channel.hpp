// control — the loopback HTTP control channel.
//
// It is how an automated run drives the shell: input, state, and frames,
// without a controller and without a compositor in the way. The shell is the
// product; this is a lens onto it, so it owns routes and nothing else, and
// every action it can take is one a controller could also take.
//
// Two things it deliberately cannot do. It never names a file to read or write:
// frames come back as bytes over the response. And it never draws: the OpenGL
// context belongs to the main loop, so a frame request is handed across and
// answered there.
#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "gamepad/reader.hpp"
#include "lucent/http.h"

namespace iideck::app {

/// The shell's state as of the last frame, published so any thread can read it.
struct ShellSnapshot {
    std::size_t games{0};
    std::size_t installed{0};
    std::size_t focusIndex{0};
    std::size_t page{0};
    std::size_t pageCount{1};
    std::string focusedId;
    std::string focusedTitle;
    std::string status;
    std::string toast;
    bool toastIsError{false};
    bool launching{false};
    /// Whether the shell has given the screen to a running game, as the loop applied it.
    bool inGame{false};
    /// Whether the Guide menu is open over that game.
    bool gameMenuOpen{false};
    /// The Steam client's state: stopped, initializing, ready, failed or blocked.
    std::string steam{"stopped"};
};

/// What the control channel may ask the shell to do. Implemented by the shell,
/// so the channel holds no shell state and can be tested on its own.
class ControlTarget {
  public:
    virtual ~ControlTarget() = default;

    /// The shell's state. Callable from any thread.
    [[nodiscard]] virtual ShellSnapshot snapshot() const = 0;

    /// Queues a button press, as if it came from a controller.
    virtual void inject(gamepad::Button button) = 0;

    /// Renders the next frame to a PNG and answers when it is written. Blocks
    /// until the main loop has done it, or fails if it cannot.
    [[nodiscard]] virtual bool captureFrame(std::string& png) = 0;

    /// Asks the main loop to shut down cleanly.
    virtual void requestClose() = 0;
};

/// Owns the listener. Loopback only.
class ControlChannel {
  public:
    ControlChannel(ControlTarget& target, std::uint16_t port);
    ~ControlChannel();

    ControlChannel(const ControlChannel&) = delete;
    ControlChannel& operator=(const ControlChannel&) = delete;

    /// Binds the listener. False means the shell still runs without a channel,
    /// which is a logged error rather than a reason to refuse to start.
    bool start();

    void stop();

    [[nodiscard]] std::uint16_t port() const noexcept;

    /// Routes a request. Exposed so the routing is testable without a socket.
    [[nodiscard]] lucent::http::Response handle(const lucent::http::Request& request);

  private:
    ControlTarget& target_;
    lucent::http::Server server_;
};

/// Parses a button name as the control channel spells it. Returns false for an
/// unknown name rather than defaulting to a button that would do something.
[[nodiscard]] bool parseButton(std::string_view name, gamepad::Button& out);

} // namespace iideck::app
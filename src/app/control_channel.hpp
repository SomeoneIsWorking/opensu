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

#include "gamepad/event.hpp"
#include "input/key_names.hpp"
#include "input/last_device.hpp"
#include "lucent/http.h"
#include "sign_in.hpp"

namespace opensu::app {

/// The shell's state as of the last frame, published so any thread can read it.
struct ShellSnapshot {
    std::size_t games{0};
    std::size_t installed{0};
    std::size_t focusIndex{0};
    std::size_t page{0};
    std::size_t pageCount{1};
    std::string focusedId;
    std::string focusedTitle;
    /// The section on screen, "home" or "library".
    std::string section{"home"};
    /// How Library lays out its tiles: "standard", "xmb" or "carousel".
    std::string libraryMode{"standard"};
    /// Whether the Library options panel is up.
    bool modeChooserOpen{false};
    /// Whether the search panel is up, and the text in it (or the search still applied).
    bool searchOpen{false};
    std::string searchText;
    /// Whether a context menu is up.
    bool contextMenuOpen{false};
    /// Whether a game's details page is up.
    bool detailsOpen{false};
    /// Whether the Settings screen is up, and whether a folder chooser or its keyboard is over it.
    bool settingsOpen{false};
    bool folderPickerOpen{false};
    /// Whether the Settings screen is waiting for a new shortcut.
    bool capturingShortcut{false};
    /// The system volume as last read (0 to 100) and whether it is muted.
    std::size_t volumePercent{0};
    bool volumeMuted{false};
    /// Whether the volume display is up.
    bool volumeShown{false};
    /// The interface size in percent.
    std::size_t uiScale{100};
    /// Where the shell is, as the top bar's trail reads: "Library > GOG > Search \"had\"".
    std::string breadcrumb;
    /// iiSU's icon size level Library is laid out at.
    std::size_t iconSize{9};
    /// The open folder's key, else the section's.
    std::string shelf{"home"};
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
    /// The launcher badges, as `steam=ready epic=failed`.
    std::string launchers;
    /// Which device the prompts name: "pad" or "keyboard".
    std::string inputDevice{"pad"};
};

/// What the control channel may ask the shell to do. Implemented by the shell,
/// so the channel holds no shell state and can be tested on its own.
class ControlTarget {
  public:
    virtual ~ControlTarget() = default;

    /// The shell's state. Callable from any thread.
    [[nodiscard]] virtual ShellSnapshot snapshot() const = 0;

    /// Types `text` as a physical keyboard would, into the search field if it is open.
    virtual void typeText(std::string text) = 0;

    /// Queues a button press, as if it came from `device`.
    virtual void inject(gamepad::Button button, input::Device device) = 0;

    /// Queues a key combination pressed on the keyboard, to be read through the shortcut table.
    virtual void injectKey(input::Combo combo) = 0;

    /// Renders the next frame to a PNG and answers when it is written. Blocks
    /// until the main loop has done it, or fails if it cannot.
    [[nodiscard]] virtual bool captureFrame(std::string& png) = 0;

    /// Asks the main loop to shut down cleanly.
    virtual void requestClose() = 0;

    /// Asks the main loop to read the library again and show `toast`. Callable from any thread.
    virtual void requestCatalogReload(std::string toast) = 0;
};

/// Owns the listener. Loopback only.
class ControlChannel {
  public:
    ControlChannel(ControlTarget& target, SignInService& signIn, std::uint16_t port);
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
    /// `/signin/<store>` finishes a sign-in with the code in the body, `/signin/<store>/start`
    /// opens the store's sign-in page.
    [[nodiscard]] lucent::http::Response signIn(const lucent::http::Request& request);

    ControlTarget& target_;
    SignInService& signIn_;
    lucent::http::Server server_;
};

} // namespace opensu::app
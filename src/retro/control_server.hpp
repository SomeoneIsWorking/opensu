// control_server — opensu-retro's loopback control channel for the shell: the control file that
// announces its port (or the load failure), the routes, and the hand-over that runs each command
// on the frame loop between frames, never beside retro_run.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "gamepad/pad_translator.hpp"
#include "resume_state.hpp"
#include "settings/video_settings.hpp"
#include "touch/touch_surface.hpp"

#include "lucent/http.h"

namespace opensu::retro {

enum class ControlCommand : std::uint8_t {
    SaveState,
    LoadState,
    Reset,
    /// Show frames under `ControlRequest::video` from the next one.
    Video,
    /// Rotate screen.
    Turn,
    /// The last image shown, as PNG.
    Frame,
    Options,
    Option,
    /// Save and close: the resume state taken and the save RAM written; the game runs no more
    /// frames while the shell closes it.
    Suspend,
    /// The shell's Guide came over the game: its frames stop, the picture stays.
    Hold,
    /// The Guide left.
    Release,
    /// The game ends as if its window closed: the save RAM written, then the runner exits.
    Quit,
};

/// A command for the frame loop and what it carries.
struct ControlRequest {
    ControlCommand command{};
    /// `Video`'s settings.
    settings::VideoSettings video;
    std::string key;
    std::string value;
};

/// What a command came to: an HTTP status and, unless it is 200, the reason.
struct ControlAnswer {
    int status{200};
    std::string reason;
    /// Whether the state slot holds a state once the command ran.
    bool hasState{};
    /// `Frame`'s image.
    std::string png{};
    std::string json{};
};

/// What `POST /input` holds on a control pad until the next one: its buttons and sticks, or its
/// leaving. Each named pad is a pad of its own; the unnamed one is the control pad.
struct ControlInput {
    std::string pad;
    std::vector<gamepad::Button> buttons;
    gamepad::Sticks sticks;
    bool leave{false};
};

/// What `POST /host` says, a hosted game's window being the shell's: each part only when it
/// changed.
struct HostInput {
    struct Key {
        /// An SDL_Scancode.
        int scancode{0};
        bool down{false};
    };

    /// A menu of the shell's is over the game, or no longer.
    std::optional<bool> covered;
    /// How many pads the shell hears.
    std::optional<unsigned> pads;
    /// The screen the game is on: its safe area and pixels per dp.
    std::optional<touch::WindowSurface> screen;
    std::optional<Key> key;
};

/// The frames a core draws as `/status` reports them.
struct ShownPicture {
    unsigned width{0};
    unsigned height{0};
    float aspect{0.0F};
    /// The player's sampling is nearest neighbour.
    bool nearest{false};
};

class ControlServer {
  public:
    /// How long a request waits for the frame loop to take it before it is answered 503.
    static constexpr std::chrono::seconds loopWait{3};

    /// `file` is where the port is announced; `core` and `rom` are what /status names.
    ControlServer(std::filesystem::path file, std::string core, std::string rom);
    /// Answers any waiting request 503, stops listening and removes the control file.
    ~ControlServer();
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;
    ControlServer(ControlServer&&) = delete;
    ControlServer& operator=(ControlServer&&) = delete;

    /// Listens on a free loopback port and writes `{"port": N, "pid": P}` to the control file.
    /// False, logged, when either fails; the game then runs without a channel.
    [[nodiscard]] bool start();
    /// Names the UDP port a netplay session is bound to in the control file; before `start`.
    void announceNetplayPort(std::uint16_t port) noexcept {
        netplayPort_ = port;
    }

    /// Keeps the player's Rotate screen choice in the control file, which stays for the shell to
    /// read when the channel closes. Frame loop only.
    void noteTurned(bool turned);

    /// What /status reports from now on. Frame loop only.
    void publish(std::uint64_t frame, bool hasState);
    /// A frame reached the target, which /status reports from now on.
    void reportShown();
    /// The core's picture, 0 by 0 when it is turned, and whether it is sampled nearest, which
    /// /status reports from now on.
    void reportPicture(const ShownPicture& picture);
    /// Whether /native asked the window to take the picture's size since the last call.
    [[nodiscard]] bool takeNative();
    /// The machine's panel (`touch::panelText`), which /status reports from now on.
    void reportPanel(std::string panel);
    /// How the game's start went with its resume state, which /status reports from now on.
    void reportResume(const ResumeReport& report);

    /// Runs every command waiting, on the calling thread. Frame loop only, between frames.
    void serve(const std::function<ControlAnswer(const ControlRequest&)>& perform);

    /// The control pad's input posted since the last call, if any. Frame loop only.
    [[nodiscard]] std::vector<ControlInput> takeInput();
    /// The fingers posted since the last call, if any. Frame loop only.
    [[nodiscard]] std::vector<touch::Finger> takeFingers();
    /// What the shell said of a hosted game's window since the last call. Frame loop only.
    [[nodiscard]] std::vector<HostInput> takeHost();
    /// What `GET /touch` answers from now on: where the touch controls are, as JSON.
    void publishTouch(std::string places);

    /// Leaves `{"error": reason}` in `file` for the shell, which removes it; logged when it cannot.
    static void reportLoadFailure(const std::filesystem::path& file, const std::string& reason);

    /// Routes a request. Any thread.
    [[nodiscard]] lucent::http::Response handle(const lucent::http::Request& request);

  private:
    /// The control file's text while the channel is open.
    [[nodiscard]] std::string announcement() const;
    struct Pending {
        ControlRequest request;
        bool taken{false};
        bool answered{false};
        ControlAnswer answer;
    };

    [[nodiscard]] lucent::http::Response run(ControlRequest request);

    std::filesystem::path file_;
    std::optional<std::uint16_t> netplayPort_;
    /// The player's Rotate screen choice; nothing until they press it.
    std::optional<bool> turned_;
    std::string core_;
    std::string rom_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<std::shared_ptr<Pending>> queue_;
    std::uint64_t frame_{0};
    ResumeReport resume_;
    std::vector<ControlInput> input_;
    std::vector<touch::Finger> fingers_;
    std::vector<HostInput> host_;
    std::string touchPlaces_{"[]"};
    bool closing_{false};
    bool hasState_{false};
    bool announced_{false};
    bool shown_{false};
    std::string panel_;
    ShownPicture picture_;
    bool nativeAsked_{false};
    /// Last, so its handler threads stop before the members they use go.
    lucent::http::Server server_;
};

} // namespace opensu::retro

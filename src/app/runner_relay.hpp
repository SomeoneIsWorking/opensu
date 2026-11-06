// runner_relay — the player's input to a game hosted in the shell's window. The runner has no
// window of its own to hear them, so SDL's events are watched as they are pushed, whichever thread
// pushes them and however the shell's loop is doing, and posted in order, off every thread that
// matters, to its control channel: `/input` for pads, `/touch` for fingers and `/host` for the keys
// and what the runner cannot see of the shell. A menu of the shell's or the window losing the
// input rests the relay: the game is told it is covered and every pad let go. Guide and Back stay
// with the shell.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL_events.h>

#include "gamepad/sdl_pads.hpp"
#include "touch/touch_surface.hpp"

namespace opensu::app {

class RunnerRelay {
  public:
    /// Delivers one post to the runner; called on the relay's thread.
    using Send = std::function<void(const std::string& route, const std::string& body)>;

    explicit RunnerRelay(Send send);
    RunnerRelay(const RunnerRelay&) = delete;
    RunnerRelay& operator=(const RunnerRelay&) = delete;
    RunnerRelay(RunnerRelay&&) = delete;
    RunnerRelay& operator=(RunnerRelay&&) = delete;
    ~RunnerRelay();

    /// A hosted game is on its way: its input is watched from now on.
    void open();
    /// It is over; what was not posted is dropped.
    void close();
    [[nodiscard]] bool isOpen() const;

    /// The shell's frame: whether the game shows, a menu of the shell's is over it, how many pads
    /// there are and the screen it is on.
    void follow(bool running, bool menu, std::size_t pads, const touch::WindowSurface& screen);
    /// Whether the player's input goes to the game now, and not to the shell.
    [[nodiscard]] bool forwarding() const;

    /// One SDL event; any thread.
    void hear(const SDL_Event& event);

  private:
    struct Pad {
        gamepad::SdlPadState state;
        /// What holds each control: the d-pad and the stick can both hold Up.
        std::map<gamepad::Button, int> presses;
    };
    struct Post {
        std::string route;
        std::string body;
        /// Which pad's buttons it carries, so a stick's next step may replace it.
        std::string pad;
        std::vector<gamepad::Button> buttons;
    };

    static bool SDLCALL watch(void* self, SDL_Event* event);
    void run(const std::stop_token& stop);
    /// Whether input goes through has changed: the runner is told, and every pad's state stands
    /// for what it holds now.
    void settle();
    void post(std::string route, std::string body);
    void postPad(SDL_JoystickID id, const Pad& pad);
    void heardPad(SDL_JoystickID id, const std::vector<gamepad::Event>& controls);

    Send send_;
    mutable std::mutex mutex_;
    std::condition_variable_any changed_;
    std::deque<Post> queue_;
    std::map<SDL_JoystickID, Pad> pads_;
    bool open_{false};
    bool running_{false};
    bool menu_{false};
    bool focused_{true};
    bool forwarding_{false};
    std::optional<std::size_t> padCount_;
    touch::WindowSurface screen_{};
    bool placed_{false};
    /// Last, so it stops before anything it reads goes.
    std::jthread thread_;
};

} // namespace opensu::app

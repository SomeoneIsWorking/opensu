#include "runner_relay.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <nlohmann/json.hpp>

#include "gamepad/pad_translator.hpp"
#include "lucent/log.h"
#include "touch/sdl_touch.hpp"

namespace opensu::app {
namespace {

std::string padName(SDL_JoystickID id) {
    return "sdl:" + std::to_string(id);
}

nlohmann::json stick(std::int32_t x, std::int32_t y) {
    const auto travel = [](std::int32_t value) {
        const std::int32_t most =
            value < 0 ? -gamepad::virtualStick.min : gamepad::virtualStick.max;
        return static_cast<double>(value) / static_cast<double>(most);
    };
    return nlohmann::json::array({travel(x), travel(y)});
}

std::string_view phaseOf(touch::Finger::Phase phase) {
    switch (phase) {
    case touch::Finger::Phase::Down:
        return "down";
    case touch::Finger::Phase::Move:
        return "move";
    case touch::Finger::Phase::Up:
        return "up";
    }
    return "up";
}

bool same(const touch::WindowSurface& one, const touch::WindowSurface& other) {
    return one.width == other.width && one.height == other.height && one.safeX == other.safeX &&
           one.safeY == other.safeY && one.safeWidth == other.safeWidth &&
           one.safeHeight == other.safeHeight && one.dp == other.dp;
}

} // namespace

RunnerRelay::RunnerRelay(Send send) : send_{std::move(send)} {
}

RunnerRelay::~RunnerRelay() {
    close();
}

bool SDLCALL RunnerRelay::watch(void* self, SDL_Event* event) {
    static_cast<RunnerRelay*>(self)->hear(*event);
    return true;
}

void RunnerRelay::open() {
    {
        const std::lock_guard lock{mutex_};
        if (open_) {
            return;
        }
        open_ = true;
        running_ = false;
        menu_ = false;
        focused_ = true;
        forwarding_ = false;
        padCount_.reset();
        placed_ = false;
        pads_.clear();
        queue_.clear();
    }
    thread_ = std::jthread{[this](const std::stop_token& stop) {
        run(stop);
    }};
    if (!SDL_AddEventWatch(&RunnerRelay::watch, this)) {
        lucent::warn("retro", "the game's input cannot be watched: {}", SDL_GetError());
    }
    lucent::info("retro", "the game's input goes through the shell");
}

void RunnerRelay::close() {
    {
        const std::lock_guard lock{mutex_};
        if (!open_) {
            return;
        }
        open_ = false;
        forwarding_ = false;
        queue_.clear();
    }
    SDL_RemoveEventWatch(&RunnerRelay::watch, this);
    thread_ = {};
}

bool RunnerRelay::isOpen() const {
    const std::lock_guard lock{mutex_};
    return open_;
}

bool RunnerRelay::forwarding() const {
    const std::lock_guard lock{mutex_};
    return forwarding_;
}

void RunnerRelay::follow(bool running, bool menu, std::size_t pads,
                         const touch::WindowSurface& screen) {
    const std::lock_guard lock{mutex_};
    if (!open_) {
        return;
    }
    running_ = running;
    menu_ = menu;
    if (!running) {
        settle();
        return;
    }
    if (padCount_ != pads) {
        padCount_ = pads;
        post("/host", nlohmann::json{{"pads", pads}}.dump());
    }
    if (!placed_ || !same(screen_, screen)) {
        placed_ = true;
        screen_ = screen;
        post("/host", nlohmann::json{{"screen",
                                      {{"safeX", screen.safeX},
                                       {"safeY", screen.safeY},
                                       {"safeWidth", screen.safeWidth},
                                       {"safeHeight", screen.safeHeight},
                                       {"dp", screen.dp}}}}
                          .dump());
    }
    settle();
}

void RunnerRelay::settle() {
    const bool through = open_ && running_ && !menu_ && focused_;
    if (through == forwarding_) {
        return;
    }
    forwarding_ = through;
    post("/host", nlohmann::json{{"covered", !through}}.dump());
    for (const auto& [id, pad] : pads_) {
        postPad(id, pad);
    }
}

void RunnerRelay::post(std::string route, std::string body) {
    queue_.push_back(Post{std::move(route), std::move(body), {}, {}});
    changed_.notify_one();
}

void RunnerRelay::postPad(SDL_JoystickID id, const Pad& pad) {
    std::vector<gamepad::Button> buttons;
    if (forwarding_) {
        for (const auto& [button, presses] : pad.presses) {
            if (presses > 0 && button != gamepad::Button::Guide) {
                buttons.push_back(button);
            }
        }
    }
    nlohmann::json names = nlohmann::json::array();
    for (const gamepad::Button button : buttons) {
        names.push_back(std::string{gamepad::name(button)});
    }
    const gamepad::Sticks sticks = forwarding_ ? pad.state.axes().sticks : gamepad::Sticks{};
    std::string name = padName(id);
    std::string body = nlohmann::json{{"pad", name},
                                      {"buttons", std::move(names)},
                                      {"left", stick(sticks.leftX, sticks.leftY)},
                                      {"right", stick(sticks.rightX, sticks.rightY)}}
                           .dump();
    // A stick moves far oftener than the runner reads; a press is never folded away.
    if (!queue_.empty() && queue_.back().pad == name && queue_.back().buttons == buttons) {
        queue_.back().body = std::move(body);
        return;
    }
    queue_.push_back(Post{"/input", std::move(body), std::move(name), std::move(buttons)});
    changed_.notify_one();
}

void RunnerRelay::heardPad(SDL_JoystickID id, const std::vector<gamepad::Event>& controls) {
    Pad& pad = pads_[id];
    for (const gamepad::Event& control : controls) {
        if (control.kind == gamepad::Event::Kind::Button) {
            int& presses = pad.presses[control.button];
            presses = control.pressed ? presses + 1 : std::max(presses - 1, 0);
        }
    }
    if (forwarding_) {
        postPad(id, pad);
    }
}

void RunnerRelay::hear(const SDL_Event& event) {
    const std::lock_guard lock{mutex_};
    if (!open_) {
        return;
    }
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        std::vector<gamepad::Event> controls;
        pads_[event.gbutton.which].state.button(
            static_cast<SDL_GamepadButton>(event.gbutton.button), event.gbutton.down, controls);
        heardPad(event.gbutton.which, controls);
        break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        std::vector<gamepad::Event> controls;
        pads_[event.gaxis.which].state.axis(static_cast<SDL_GamepadAxis>(event.gaxis.axis),
                                            event.gaxis.value, controls);
        heardPad(event.gaxis.which, controls);
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (pads_.erase(event.gdevice.which) != 0) {
            post("/input",
                 nlohmann::json{{"pad", padName(event.gdevice.which)}, {"leave", true}}.dump());
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (forwarding_) {
            const touch::Finger finger =
                touch::fingerOf(event.tfinger, screen_.width, screen_.height);
            post("/touch", nlohmann::json{{"finger", finger.id},
                                          {"phase", phaseOf(finger.phase)},
                                          {"x", finger.x},
                                          {"y", finger.y}}
                               .dump());
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (forwarding_ && !event.key.repeat && event.key.scancode != SDL_SCANCODE_AC_BACK) {
            post("/host", nlohmann::json{{"key",
                                          {{"scancode", static_cast<int>(event.key.scancode)},
                                           {"down", event.type == SDL_EVENT_KEY_DOWN}}}}
                              .dump());
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        focused_ = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        settle();
        break;
    default:
        break;
    }
}

void RunnerRelay::run(const std::stop_token& stop) {
    std::unique_lock lock{mutex_};
    while (changed_.wait(lock, stop, [this] {
        return !queue_.empty();
    })) {
        Post next = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        send_(next.route, next.body);
        lock.lock();
    }
}

} // namespace opensu::app

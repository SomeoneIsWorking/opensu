#include "control_bridge.hpp"

#include <algorithm>
#include <chrono>

#include "gamepad/direction_repeat.hpp"

namespace opensu::app {

ShellSnapshot ControlBridge::snapshot() const {
    const std::lock_guard lock{stateMutex_};
    return published_;
}

void ControlBridge::publish(ShellSnapshot snapshot) {
    const std::lock_guard lock{stateMutex_};
    published_ = std::move(snapshot);
}

void ControlBridge::inject(const std::vector<gamepad::Button>& buttons, input::Device device,
                           std::chrono::milliseconds hold, bool stick) {
    const auto edge = [stick](gamepad::Button button, bool pressed) {
        return gamepad::Event{.kind = gamepad::Event::Kind::Button,
                              .button = button,
                              .pressed = pressed,
                              .stick = stick && gamepad::DirectionRepeat::repeats(button)};
    };
    const Clock::time_point now = Clock::now();
    const std::lock_guard lock{inputMutex_};
    for (const gamepad::Button button : buttons) {
        edges_.push_back(Edge{now, edge(button, true), device});
    }
    for (auto button = buttons.rbegin(); button != buttons.rend(); ++button) {
        edges_.push_back(Edge{now + hold, edge(*button, false), device});
    }
}

void ControlBridge::injectKey(input::Combo combo) {
    const std::lock_guard lock{inputMutex_};
    input_.keys.push_back(combo);
}

void ControlBridge::injectPointer(PointerInjection pointer) {
    const std::lock_guard lock{inputMutex_};
    input_.pointer.push_back(pointer);
}

void ControlBridge::typeText(std::string text) {
    const std::lock_guard lock{inputMutex_};
    input_.text.push_back(std::move(text));
}

ControlBridge::Input ControlBridge::takeInput(Clock::time_point now) {
    const std::lock_guard lock{inputMutex_};
    Input taken = std::exchange(input_, {});
    // Edges leave in the order they were queued, so none overtakes a held button's release.
    const auto waiting = std::ranges::find_if(edges_, [now](const Edge& edge) {
        return edge.due > now;
    });
    for (auto edge = edges_.begin(); edge != waiting; ++edge) {
        taken.buttons.emplace_back(edge->event, edge->device);
    }
    edges_.erase(edges_.begin(), waiting);
    return taken;
}

void ControlBridge::requestClose() {
    closeRequested_.store(true);
}

void ControlBridge::requestCatalogReload(std::string toast) {
    const std::lock_guard lock{reloadMutex_};
    reload_ = std::move(toast);
}

std::optional<std::string> ControlBridge::takeReload() {
    const std::lock_guard lock{reloadMutex_};
    return std::exchange(reload_, std::nullopt);
}

bool ControlBridge::captureFrame(std::string& png) {
    std::unique_lock lock{stateMutex_};
    if (framePending_) {
        // One frame request at a time: two callers racing for the context would
        // interleave, and the second would get the first's answer.
        return false;
    }
    framePending_ = true;
    frame_.clear();
    // The loop answers within a frame or two. The bound stops a caller hanging
    // forever if the loop has already exited.
    frameAnswered_.wait_for(lock, std::chrono::seconds{5}, [this] {
        return !framePending_;
    });
    if (framePending_) {
        // Timed out, so the request is abandoned rather than left set.
        framePending_ = false;
        return false;
    }
    png = std::move(frame_);
    frame_.clear();
    return !png.empty();
}

bool ControlBridge::frameWanted() const {
    const std::lock_guard lock{stateMutex_};
    return framePending_;
}

void ControlBridge::answerFrame(std::string png) {
    const std::lock_guard lock{stateMutex_};
    frame_ = std::move(png);
    framePending_ = false;
    frameAnswered_.notify_all();
}

void ControlBridge::abandonFrame() {
    const std::lock_guard lock{stateMutex_};
    framePending_ = false;
    frameAnswered_.notify_all();
}

} // namespace opensu::app

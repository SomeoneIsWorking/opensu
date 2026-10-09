#include "control_bridge.hpp"

#include <chrono>

namespace opensu::app {

ShellSnapshot ControlBridge::snapshot() const {
    const std::lock_guard lock{stateMutex_};
    return published_;
}

void ControlBridge::publish(ShellSnapshot snapshot) {
    const std::lock_guard lock{stateMutex_};
    published_ = std::move(snapshot);
}

void ControlBridge::inject(gamepad::Button button, input::Device device) {
    const std::lock_guard lock{inputMutex_};
    input_.buttons.emplace_back(button, device);
}

void ControlBridge::injectKey(input::Combo combo) {
    const std::lock_guard lock{inputMutex_};
    input_.keys.push_back(combo);
}

void ControlBridge::typeText(std::string text) {
    const std::lock_guard lock{inputMutex_};
    input_.text.push_back(std::move(text));
}

ControlBridge::Input ControlBridge::takeInput() {
    const std::lock_guard lock{inputMutex_};
    return std::exchange(input_, {});
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

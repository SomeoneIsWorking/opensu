// control_bridge — the hand-over between the control channel's threads and the main loop. The
// channel queues input, asks for frames and reads the published state from its threads; the loop
// takes the input, answers the frame requests and publishes the state. It owns the queues, the
// snapshot and the frame handshake, and nothing else.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "control_channel.hpp"

namespace opensu::app {

class ControlBridge final : public ControlTarget {
  public:
    /// A key combination pressed or text typed on the keyboard.
    using Keystroke = std::variant<input::Combo, std::string>;

    /// What the channel queued since the loop last took it.
    struct Input {
        /// Presses and releases, each with the device it is from.
        std::vector<std::pair<gamepad::Event, input::Device>> buttons;
        /// In the order they were asked for.
        std::vector<Keystroke> keystrokes;
        std::vector<PointerInjection> pointer;
        /// The newest remote link cap asked for, if any.
        std::optional<std::uint64_t> linkCap;
        /// The newest text to put on the system clipboard, if any.
        std::optional<std::string> clipboard;
    };

    // ControlTarget. Each is callable from any thread.
    [[nodiscard]] ShellSnapshot snapshot() const override;
    void typeText(std::string text) override;
    void setClipboard(std::string text) override;
    void inject(const std::vector<gamepad::Button>& buttons, input::Device device,
                std::chrono::milliseconds hold, bool stick) override;
    void injectKey(input::Combo combo) override;
    void injectPointer(PointerInjection pointer) override;
    void capRemoteLink(std::uint64_t bitsPerSecond) override;
    [[nodiscard]] bool captureFrame(FrameKind kind, FramePng& frame) override;
    void requestClose() override;
    /// Asks the loop to read the library again; `toast` is shown when it is not empty.
    void requestCatalogReload(std::string toast) override;

    // The loop's side.
    using Clock = std::chrono::steady_clock;
    /// What was queued, the button edges up to the first one not due by `now` (a held button's
    /// release, and whatever was queued after it).
    [[nodiscard]] Input takeInput(Clock::time_point now);
    [[nodiscard]] bool closeRequested() const noexcept {
        return closeRequested_.load();
    }
    /// The toast of a requested reload (empty for none), or nothing when none was requested.
    [[nodiscard]] std::optional<std::string> takeReload();
    void publish(ShellSnapshot snapshot);
    /// The kind of frame a caller waits for, or nothing.
    [[nodiscard]] std::optional<FrameKind> frameWanted() const;
    /// Hands the waiting caller its frame; empty `png` says it could not be drawn.
    void answerFrame(FramePng frame);
    /// The loop ended: nobody waiting will get a frame.
    void abandonFrame();

  private:
    std::atomic<bool> closeRequested_{false};

    std::mutex reloadMutex_;
    std::optional<std::string> reload_;

    /// A button edge and when it is due.
    struct Edge {
        Clock::time_point due;
        gamepad::Event event;
        input::Device device;
    };

    std::mutex inputMutex_;
    /// The keystrokes and pointer; the buttons wait in `edges_` until due.
    Input input_;
    std::vector<Edge> edges_;

    /// The published state and the frame-request handshake.
    mutable std::mutex stateMutex_;
    ShellSnapshot published_;
    std::condition_variable frameAnswered_;
    std::optional<FrameKind> framePending_;
    FramePng frame_;
};

} // namespace opensu::app

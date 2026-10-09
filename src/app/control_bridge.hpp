// control_bridge — the hand-over between the control channel's threads and the main loop. The
// channel queues input, asks for frames and reads the published state from its threads; the loop
// takes the input, answers the frame requests and publishes the state. It owns the queues, the
// snapshot and the frame handshake, and nothing else.
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "control_channel.hpp"

namespace opensu::app {

class ControlBridge final : public ControlTarget {
  public:
    /// What the channel queued since the loop last took it.
    struct Input {
        std::vector<std::pair<gamepad::Button, input::Device>> buttons;
        std::vector<input::Combo> keys;
        std::vector<std::string> text;
    };

    // ControlTarget. Each is callable from any thread.
    [[nodiscard]] ShellSnapshot snapshot() const override;
    void typeText(std::string text) override;
    void inject(gamepad::Button button, input::Device device) override;
    void injectKey(input::Combo combo) override;
    [[nodiscard]] bool captureFrame(std::string& png) override;
    void requestClose() override;
    /// Asks the loop to read the library again; `toast` is shown when it is not empty.
    void requestCatalogReload(std::string toast) override;

    // The loop's side.
    [[nodiscard]] Input takeInput();
    [[nodiscard]] bool closeRequested() const noexcept {
        return closeRequested_.load();
    }
    /// The toast of a requested reload (empty for none), or nothing when none was requested.
    [[nodiscard]] std::optional<std::string> takeReload();
    void publish(ShellSnapshot snapshot);
    /// Whether a caller waits for a frame.
    [[nodiscard]] bool frameWanted() const;
    /// Hands the waiting caller its frame; empty `png` says it could not be drawn.
    void answerFrame(std::string png);
    /// The loop ended: nobody waiting will get a frame.
    void abandonFrame();

  private:
    std::atomic<bool> closeRequested_{false};

    std::mutex reloadMutex_;
    std::optional<std::string> reload_;

    std::mutex inputMutex_;
    Input input_;

    /// The published state and the frame-request handshake.
    mutable std::mutex stateMutex_;
    ShellSnapshot published_;
    std::condition_variable frameAnswered_;
    bool framePending_{false};
    std::string frame_;
};

} // namespace opensu::app

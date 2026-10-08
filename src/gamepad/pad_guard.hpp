// pad_guard — holds every gamepad while a game runs: the physical pads are grabbed, the game reads
// one virtual Xbox 360 pad per pad, and iideck alone sees Guide. While blocked, the virtual pads
// rest, so the Guide menu's input never reaches the game.
#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "evdev_device.hpp"
#include "event.hpp"
#include "pad_translator.hpp"
#include "virtual_pad.hpp"

namespace iideck::gamepad {

class PadGuard {
  public:
    /// Grabs every gamepad node in `inputDir` and starts forwarding.
    explicit PadGuard(const std::filesystem::path& inputDir = "/dev/input");
    PadGuard(const PadGuard&) = delete;
    PadGuard& operator=(const PadGuard&) = delete;
    /// Releases the pads; their virtual pads disappear.
    ~PadGuard();

    /// How many pads were held at the start.
    [[nodiscard]] std::size_t held() const {
        return held_;
    }

    /// What a game started now needs in its environment to read only the virtual pads: SDL hides
    /// every other controller, the grabbed ones included. Empty when a pad could not be held,
    /// since hiding it would leave the game without it.
    [[nodiscard]] std::vector<std::string> environment() const;

    /// Rests the virtual pads while blocked; on unblocking they take up the pads' axes again.
    void setBlocked(bool blocked);

    /// The controls pressed and released since the last call.
    [[nodiscard]] std::vector<Event> takeControls();

  private:
    struct Pad {
        explicit Pad(EvdevDevice held);

        EvdevDevice device;
        VirtualPad output;
        PadTranslator translator;
    };

    void run(const std::stop_token& stop);

    /// Owned by the forwarding thread once it starts.
    std::vector<std::unique_ptr<Pad>> pads_;
    std::size_t held_{0};
    bool missed_{false};
    int wake_{-1};
    std::atomic<bool> blocked_{false};
    std::mutex controlsMutex_;
    std::vector<Event> controls_;
    std::jthread thread_;
};

} // namespace iideck::gamepad

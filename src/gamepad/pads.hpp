// pads — every controller, read from evdev for as long as iideck runs. A pad connected later is
// picked up when its node appears. While a game runs the pads are held: grabbed, with the game
// reading one virtual Xbox 360 pad per pad, and Guide reaching iideck alone. While blocked, the
// virtual pads rest, so the Guide menu's input never reaches the game.
#pragma once

#include <condition_variable>
#include <cstdint>
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

class Pads {
  public:
    /// Reads every pad node in `inputDir`, now and as they appear. Throws std::system_error when
    /// the directory cannot be watched.
    explicit Pads(std::filesystem::path inputDir = "/dev/input");
    Pads(const Pads&) = delete;
    Pads& operator=(const Pads&) = delete;
    ~Pads();

    /// Control presses and releases, and pads connecting and leaving, since the last call.
    [[nodiscard]] std::vector<Event> takeEvents();

    /// Holds every pad for a game, including pads that connect while held. Returns what the game
    /// needs in its environment to read only the virtual pads: SDL hides every other controller,
    /// the grabbed ones included. Empty when a pad could not be held, since hiding it would leave
    /// the game without it.
    std::vector<std::string> hold();
    /// Gives the pads back to everyone; the virtual pads disappear.
    void release();

    /// Rests the virtual pads while blocked; on unblocking they take up the pads' axes again.
    void setBlocked(bool blocked);

  private:
    struct Pad {
        explicit Pad(EvdevDevice opened);

        EvdevDevice device;
        PadTranslator translator;
        /// The game's view of this pad, while held.
        std::unique_ptr<VirtualPad> output;
    };

    /// What the main thread asked for; applied by the reading thread.
    struct Wanted {
        bool held{false};
        bool blocked{false};
        /// Bumped per request, so a caller can wait for its own to be applied.
        std::uint64_t serial{0};
    };

    void run(const std::stop_token& stop);
    void wake() const;
    void scan();
    void add(const std::filesystem::path& node);
    /// Takes a pad into or out of the held state; false when it could not be held.
    bool apply(Pad& pad, bool held, bool blocked);
    void write(Pad& pad, const std::vector<PadEvent>& events);
    void publish(std::vector<Event>& events);

    std::filesystem::path inputDir_;
    int wake_{-1};
    int watch_{-1};

    // Reading thread only.
    std::vector<std::unique_ptr<Pad>> pads_;
    Wanted applied_;
    bool missed_{false};

    mutable std::mutex mutex_;
    std::condition_variable appliedChanged_;
    Wanted wanted_;
    std::uint64_t appliedSerial_{0};
    bool appliedMissed_{false};
    std::vector<Event> events_;

    std::jthread thread_;
};

} // namespace iideck::gamepad

// evdev_pads — every controller, read from evdev for as long as opensu runs; Linux's `PadSource`. A
// pad connected later is picked up when its node appears, and takes the claim in force. The claim
// says who else sees the pads: Free leaves them to everyone; Shell grabs them so nothing else
// (Steam above all, which opens Big Picture on Guide) sees them; Game grabs them and gives the game
// one virtual Xbox 360 pad per pad, Guide reaching opensu alone; Away lets them go while a game
// runs and another window has the focus, its virtual pads resting. opensu reads every pad in every
// claim. The buttons of a bound chord never reach the game (`ChordGate`, through each pad's
// `PadTranslator`), and the game's mapping (`ForwardMap`) applies to its virtual pads only. While
// blocked, the virtual pads rest, so the Guide menu's input never reaches the game. Only gamepad
// nodes are opened: a pad's separate motion sensor node and its hidraw node are never read or
// grabbed. Virtual pads can be kept: they exist, at rest, until let go, and under Game the pads
// drive them in connection order before any other is made, so a program given only their nodes up
// front (Isolated Steam's sandbox) still gets the game's input.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "evdev_device.hpp"
#include "event.hpp"
#include "forward_map.hpp"
#include "pad_source.hpp"
#include "pad_translator.hpp"
#include "virtual_pad.hpp"

namespace opensu::gamepad {

class EvdevPads final : public PadSource {
  public:
    /// Reads every pad node in `inputDir`, now and as they appear, unclaimed (Free).
    /// Throws std::system_error when the directory cannot be watched.
    explicit EvdevPads(std::filesystem::path inputDir = "/dev/input",
                       OwnVirtualPads own = OwnVirtualPads::Skip);
    EvdevPads(const EvdevPads&) = delete;
    EvdevPads& operator=(const EvdevPads&) = delete;
    EvdevPads(EvdevPads&&) = delete;
    EvdevPads& operator=(EvdevPads&&) = delete;
    ~EvdevPads() override;

    [[nodiscard]] bool grabs() const noexcept override {
        return true;
    }

    /// Control presses and releases, and pads connecting and leaving, since the last call.
    [[nodiscard]] std::vector<Event> takeEvents() override;

    /// The pads connected now, in the order they connected. Any thread.
    [[nodiscard]] std::vector<PadInfo> connected() const override;

    /// Where each connected pad's sticks stand now, by `PadInfo::id`. Any thread.
    [[nodiscard]] std::map<std::string, Sticks> sticks() const override;

    /// The kernel's power supply named after the pad's unique id (its Bluetooth address).
    [[nodiscard]] std::optional<device::BatteryStatus> battery(const PadInfo& pad) const override;

    /// Claims every pad for a game (Game), its virtual pads sending what `map` says. Returns what
    /// the game needs in its environment to read only the virtual pads: SDL hides every other
    /// controller, the grabbed ones included. Empty when a pad could not be grabbed, since hiding
    /// it would leave the game without it.
    std::vector<std::string> hold(ForwardMap map) override;
    /// The game is over: the map is dropped and `after` (Free or Shell) applies; the virtual pads
    /// are gone on return.
    void release(Claim after) override;
    /// Moves between claims of the same game state: Game and Away while a game runs, Shell and
    /// Free otherwise. Returns once applied.
    void claim(Claim claim) override;

    /// Rests the virtual pads while blocked; on unblocking they take up the pads' axes again.
    void setBlocked(bool blocked) override;

    /// The chords whose buttons never reach the game (`ChordGate`); returns once applied.
    void setChords(ChordTriggers chords) override;

    /// Keeps `count` virtual pads, making or ending them, and returns their nodes once applied;
    /// fewer when uinput refuses one. 0 lets every kept pad go. Any thread.
    std::vector<std::filesystem::path> keepVirtualPads(std::size_t count) override;

  private:
    struct Pad {
        explicit Pad(EvdevDevice opened);

        EvdevDevice device;
        PadTranslator translator;
        /// The game's view of this pad, under Game and Away: a kept pad, or `made`.
        VirtualPad* output{nullptr};
        /// The virtual pad made for this pad when no kept one was free.
        std::unique_ptr<VirtualPad> made;
        bool grabbed{false};
        /// The virtual pad follows the pad, rather than resting.
        bool live{false};
    };

    /// What the main thread asked for; applied by the reading thread.
    struct Wanted {
        Claim claim{Claim::Free};
        bool blocked{false};
        ChordTriggers chords;
        ForwardMap map;
        /// How many virtual pads are kept.
        std::size_t keep{0};
        /// Bumped per request, so a caller can wait for its own to be applied.
        std::uint64_t serial{0};
    };

    void run(const std::stop_token& stop);
    void wake() const;
    /// Wakes the worker and waits until it has applied `serial`.
    void awaitApplied(std::unique_lock<std::mutex>& lock, std::uint64_t serial);
    /// Sets the claim and waits until it applies.
    void request(std::unique_lock<std::mutex>& lock, Claim claim);
    void scan();
    void add(const std::filesystem::path& node);
    /// Brings a pad to `wanted`; false when it could not be grabbed.
    bool apply(Pad& pad, const Wanted& wanted);
    /// Makes or ends kept virtual pads until `count` are kept; a pad whose kept one ends is left
    /// without one, for `apply` to give it another.
    void keep(std::size_t count);
    /// A kept virtual pad no pad drives, or null.
    [[nodiscard]] VirtualPad* freeKept() const;
    void write(Pad& pad, const std::vector<PadEvent>& events);
    void publish(std::vector<Event>& events);

    std::filesystem::path inputDir_;
    OwnVirtualPads own_;
    int wake_{-1};
    int watch_{-1};

    // Reading thread only.
    std::vector<std::unique_ptr<Pad>> pads_;
    std::vector<std::unique_ptr<VirtualPad>> kept_;
    Wanted applied_;
    bool missed_{false};

    device::ControllerBatteries batteries_;
    mutable std::mutex mutex_;
    std::condition_variable appliedChanged_;
    Wanted wanted_;
    std::uint64_t appliedSerial_{0};
    bool appliedMissed_{false};
    std::vector<Event> events_;
    std::vector<PadInfo> connected_;
    std::map<std::string, Sticks> sticks_;
    std::vector<std::filesystem::path> keptNodes_;

    std::jthread thread_;
};

} // namespace opensu::gamepad

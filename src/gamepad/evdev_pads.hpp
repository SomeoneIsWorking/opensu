// evdev_pads — every controller, read from evdev for as long as opensu runs; Linux's `PadSource`. A
// pad connected later is picked up when its node appears, and takes the claim in force. The claim
// says who else sees the pads: Free leaves them to everyone; Shell grabs them so nothing else
// (Steam above all, which opens Big Picture on Guide) sees them; Game grabs them and gives the game
// one virtual Xbox 360 pad per pad, Guide reaching opensu alone; Away lets them go while a game
// runs and another window has the focus, its virtual pads resting. opensu reads every pad in every
// claim. The buttons of a bound chord never reach the game (`ChordGate`, through each pad's
// `PadTranslator`), and the game's mapping (`ForwardMap`) applies to its virtual pads only. While
// blocked, the virtual pads rest, so the Guide menu's input never reaches the game. Only gamepad
// nodes and a Wii Remote's parts are opened: a pad's separate motion sensor node and its hidraw
// node are never read or grabbed. Virtual pads can be kept: they exist, at rest, until let go, and
// under Game the pads drive them in connection order before any other is made, so a program given
// only their nodes up front (Isolated Steam's sandbox) still gets the game's input. A Bluetooth Wii
// Remote is one pad (`WiiRemoteLayout`): its Nunchuk or Classic Controller and its IR camera,
// devices of their own sharing the remote's parent device, are read as part of it, each held until
// its remote is there; where the camera says the remote points comes as Pointer events, read in the
// range set (`setWiiPointerRange`). The shell also reads each slot of a DolphinBar in Wii Remote
// mode (`WiiSlot`) as a pad while a remote answers there, asking every second, and finds the bars
// again as hidraw nodes come and go. A hold for a game that plays the real remotes itself
// (`WiiRemoteHold::ToGame`) closes every remote's devices and the bars' slots, and opens none
// until the hold ends; a remote's part is told from sysfs, never by opening it.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "evdev_device.hpp"
#include "event.hpp"
#include "forward_map.hpp"
#include "pad_source.hpp"
#include "pad_translator.hpp"
#include "virtual_pad.hpp"
#include "wii_slot.hpp"

namespace opensu::gamepad {

class EvdevPads final : public PadSource {
  public:
    /// Reads every pad node in `inputDir`, and the slots of the DolphinBars `barSlots` shows, now
    /// and as they appear, unclaimed (Free). Throws std::system_error when the directory cannot
    /// be watched.
    EvdevPads(std::filesystem::path inputDir, std::optional<device::HidPlaces> barSlots,
              OwnVirtualPads own);
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
    [[nodiscard]] std::map<std::string, PadAxes> axes() const override;

    /// The kernel's power supply named after the pad's unique id (its Bluetooth address).
    [[nodiscard]] std::optional<device::BatteryStatus> battery(const PadInfo& pad) const override;

    /// Claims every pad for a game (Game), its virtual pads sending what `map` says. Returns what
    /// the game needs in its environment to read only the virtual pads: SDL hides every other
    /// controller, the grabbed ones included. Empty when a pad could not be grabbed, since hiding
    /// it would leave the game without it.
    HeldPads hold(ForwardMap map, WiiRemoteHold remotes) override;
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
    void setWiiPointerRange(const WiiPointerRange& range) override;

    /// Keeps `count` virtual pads, making or ending them, and returns their nodes once applied;
    /// fewer when uinput refuses one. 0 lets every kept pad go. Any thread.
    std::shared_ptr<VirtualPad> lendPad() override;
    void returnPad() override;
    std::shared_ptr<VirtualPad> seatPad() override;
    std::vector<std::filesystem::path> keepVirtualPads(std::size_t count) override;

  private:
    struct Pad {
        /// An evdev pad, or a Bluetooth Wii Remote's buttons.
        explicit Pad(EvdevDevice opened);
        /// The remote in a DolphinBar's slot.
        explicit Pad(WiiSlot& slot);

        /// The pad's id (`PadInfo::id`): its node.
        [[nodiscard]] std::string id() const;
        [[nodiscard]] const std::string& name() const;
        /// The layout for what the remote has plugged in now; nothing for another pad.
        [[nodiscard]] std::optional<WiiRemoteLayout> layoutNow() const;
        /// The translator started again from the devices' present state, as `layout` reads them.
        void restart(const ChordTriggers& chords, const ForwardMap& map);
        /// The events `read` from `part` as the translator reads them.
        [[nodiscard]] std::vector<PadEvent> translatable(const std::vector<PadEvent>& read,
                                                         WiiRemotePart part) const;
        [[nodiscard]] std::vector<PadEvent>
        translatable(const std::vector<WiiPartEvent>& read) const;
        [[nodiscard]] PadCapabilities capabilities() const;

        /// The evdev device, or nothing for a bar's slot.
        std::optional<EvdevDevice> device;
        /// The bar's slot, owned by `slots_`; null for an evdev pad.
        WiiSlot* slot{nullptr};
        /// A Wii Remote's Nunchuk or Classic Controller, read as part of this pad.
        std::optional<EvdevDevice> extension;
        /// A Wii Remote's IR camera, and where it says the remote points.
        std::optional<EvdevDevice> camera;
        WiiPointer pointer;
        /// How a Wii Remote's devices read as a pad; nothing for another pad.
        std::optional<WiiRemoteLayout> layout;
        PadTranslator translator;
        /// The game's view of this pad, under Game and Away: a kept pad, or `made`.
        VirtualPad* output{nullptr};
        /// The virtual pad made for this pad when no kept one was free.
        std::shared_ptr<VirtualPad> made;
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
        WiiRemoteHold remotes{WiiRemoteHold::AsPads};
        WiiPointerRange pointer;
        /// How many virtual pads are kept.
        std::size_t keep{0};
        /// The first pad's virtual pad is lent to a remote player.
        bool lent{false};
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
    /// Makes `pad` a pad: claimed as the others are, listed and announced.
    void connect(std::unique_ptr<Pad> pad);
    /// Reads `part` (an extension or a camera) as part of the Wii Remote pad that shares its
    /// parent, or holds it until that remote is there.
    void addPart(EvdevDevice part);
    /// Gives `pad` the part when it is its remote's and that place is free; false otherwise.
    bool attach(Pad& pad, EvdevDevice& part);
    /// Gives `pad` its extension, or takes it away when `extension` is nothing.
    void setExtension(Pad& pad, std::optional<EvdevDevice> extension);
    /// Reads `pad` with the layout for what its remote has plugged in now.
    void relayout(Pad& pad);
    /// Opens the slots of the bars there are now that are not open yet, unless the remotes are
    /// the game's.
    void openSlots();
    /// Asks every slot whether a remote is there; a pad whose remote left is dropped.
    void askSlots(std::vector<Event>& events);
    /// The pad of `slot`'s remote, or null.
    [[nodiscard]] Pad* padOf(const WiiSlot& slot) const;
    /// Removes pad `index`, its virtual pad rested, reporting it gone.
    void drop(std::size_t index, std::vector<Event>& events);
    /// Closes every Wii Remote's devices and the bars' slots for a game that plays them itself.
    void letGoOfWiiRemotes();
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
    std::optional<device::HidPlaces> barSlots_;
    OwnVirtualPads own_;
    int wake_{-1};
    int watch_{-1};
    /// The watch of `inputDir_`, and of the bars' hidraw nodes (-1 when there is none).
    int inputWatch_{-1};
    int hidWatch_{-1};

    // Reading thread only.
    std::vector<std::unique_ptr<Pad>> pads_;
    /// Extensions and cameras whose remote is not here yet.
    std::vector<EvdevDevice> loose_;
    /// The bars' slots open, with a remote or not.
    std::vector<std::unique_ptr<WiiSlot>> slots_;
    /// Slots that could not be opened, said once each.
    std::set<std::filesystem::path> refusedSlots_;
    std::chrono::steady_clock::time_point asked_{};
    std::vector<std::shared_ptr<VirtualPad>> kept_;
    /// `kept_` for `seatPad`, under the mutex.
    std::vector<std::shared_ptr<VirtualPad>> seats_;
    Wanted applied_;
    bool missed_{false};

    device::ControllerBatteries batteries_;
    mutable std::mutex mutex_;
    /// The virtual pad lent, as the reading thread last left it.
    std::shared_ptr<VirtualPad> lentPad_;
    std::condition_variable appliedChanged_;
    Wanted wanted_;
    std::uint64_t appliedSerial_{0};
    bool appliedMissed_{false};
    std::vector<Event> events_;
    std::vector<PadInfo> connected_;
    std::map<std::string, PadAxes> axes_;
    std::vector<std::filesystem::path> keptNodes_;

    std::jthread thread_;
};

} // namespace opensu::gamepad

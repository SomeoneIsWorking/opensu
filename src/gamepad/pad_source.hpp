// pad_source — every controller openSU reads, whatever reads them, and who else sees them. One
// backend per OS, opened by `openPadSource` (picked in CMakeLists.txt): evdev on Linux
// (`EvdevPads`), which can grab the pads and give a game virtual pads of them; SDL's gamepads
// elsewhere (`SdlPads`), which only reads them, so its claims never take a pad from anyone.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chord_gate.hpp"
#include "event.hpp"
#include "forward_map.hpp"
#include "pad_translator.hpp"

namespace opensu::gamepad {

/// A connected controller.
struct PadInfo {
    /// Which pad it is, as `Event::source` names it: the evdev node ("/dev/input/event12"), or
    /// SDL's joystick id ("sdl:3").
    std::string id;
    std::string name;
    /// The device's unique id (a Bluetooth pad's address), or empty.
    std::string uniq;

    bool operator==(const PadInfo&) const = default;
};

/// Who else sees the pads.
enum class Claim : std::uint8_t {
    /// Everyone reads them.
    Free,
    /// Grabbed for opensu's own screens.
    Shell,
    /// Grabbed, with a virtual pad each for the game.
    Game,
    /// A game runs under another window: everyone reads them, the virtual pads rest.
    Away,
};

/// Whether a reader takes opensu's own virtual pads: the shell skips them, a game process reads
/// them.
enum class OwnVirtualPads : std::uint8_t {
    Skip,
    Read,
};

class PadSource {
  public:
    PadSource() = default;
    PadSource(const PadSource&) = delete;
    PadSource& operator=(const PadSource&) = delete;
    PadSource(PadSource&&) = delete;
    PadSource& operator=(PadSource&&) = delete;
    virtual ~PadSource() = default;

    /// Whether the claims Shell and Game take the pads from other programs and give a game
    /// virtual pads; without it they read as Free and Away.
    [[nodiscard]] virtual bool grabs() const noexcept = 0;

    /// Control presses and releases, and pads connecting and leaving, since the last call.
    [[nodiscard]] virtual std::vector<Event> takeEvents() = 0;
    /// The pads connected now, in the order they connected. Any thread.
    [[nodiscard]] virtual std::vector<PadInfo> connected() const = 0;
    /// Where each connected pad's sticks stand now, by `PadInfo::id`. Any thread.
    [[nodiscard]] virtual std::map<std::string, Sticks> sticks() const = 0;

    /// Claims every pad for a game (Game), its virtual pads sending what `map` says. Returns what
    /// the game needs in its environment to read only the virtual pads; empty when it reads the
    /// pads themselves.
    virtual std::vector<std::string> hold(ForwardMap map) = 0;
    /// The game is over: the map is dropped and `after` (Free or Shell) applies.
    virtual void release(Claim after) = 0;
    /// Moves between claims of the same game state: Game and Away while a game runs, Shell and
    /// Free otherwise. Returns once applied.
    virtual void claim(Claim claim) = 0;
    /// Rests the virtual pads while blocked; on unblocking they take up the pads' axes again.
    virtual void setBlocked(bool blocked) = 0;
    /// The chords whose buttons never reach the game (`ChordGate`); returns once applied.
    virtual void setChords(ChordTriggers chords) = 0;
    /// Keeps `count` virtual pads, making or ending them, and returns their nodes once applied;
    /// fewer when they cannot be made. 0 lets every kept pad go. Any thread.
    virtual std::vector<std::filesystem::path> keepVirtualPads(std::size_t count) = 0;
};

struct PadSourceOptions {
    /// A hidden run reads only pads linked into this directory (evdev, which creates it), or none
    /// (SDL); nothing for the system's pads.
    std::optional<std::filesystem::path> hiddenInputs{};
    OwnVirtualPads own{OwnVirtualPads::Skip};
};

/// This OS's pads. Throws std::system_error when they cannot be watched.
[[nodiscard]] std::unique_ptr<PadSource> openPadSource(const PadSourceOptions& options);

} // namespace opensu::gamepad

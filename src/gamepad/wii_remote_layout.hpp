// wii_remote_layout — a Wii Remote as an ordinary pad. Linux's hid-wiimote gives a Bluetooth remote
// one evdev device for its buttons ("Nintendo Wii Remote": the d-pad as KEY_UP and the rest, A,
// B, 1, 2, plus as KEY_NEXT, minus as KEY_PREVIOUS, Home as BTN_MODE; no axes) and one per
// extension ("Nintendo Wii Remote Nunchuk": the stick on ABS_HAT0X/Y with up positive, C, Z and
// its accelerometer; "Nintendo Wii Remote Classic Controller": the sticks on ABS_HAT1 and ABS_HAT2
// with up positive, the triggers' travel on ABS_HAT3, the buttons by their own names), all with
// the remote's USB ids. A DolphinBar slot's remote (`WiiSlot`) comes out as the same events.
// Alone, the remote is held sideways like an NES pad: the d-pad turned a quarter, 2 and 1 as
// south and east, B and A as west and north. With its Nunchuk it is held upright: A and B as
// south and east, 1 and 2 as west and north, the stick as the left stick, C and Z as the left
// shoulder and trigger. With a Classic Controller the remote hangs off it and only the Classic is
// read, its buttons by place (B south, A east, Y west, X north), as Wii games emulated on a pad
// play it (`assets/wii-controls.json`): ZL and ZR as the shoulders, L and R fully in as the
// triggers, the sticks as the sticks. Plus, minus and Home are Start, Select and Guide every way.
// Events come out in the kernel's gamepad codes, which `PadTranslator` reads. The remote's IR
// camera ("Nintendo Wii Remote IR", which the kernel turns on while it is open) says where the
// remote points (`WiiPointer`).
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

#include "pad_translator.hpp"
#include "wii_pointer_range.hpp"

namespace opensu::gamepad {

/// Which of a remote's evdev devices one is.
enum class WiiRemotePart : std::uint8_t {
    Remote,
    Nunchuk,
    Classic,
    Camera,
};

/// The part of a Wii Remote the evdev device with these ids and `name` is; nothing for anything
/// else, the remote's accelerometer and other extensions included.
[[nodiscard]] std::optional<WiiRemotePart>
wiiRemotePartOf(std::uint16_t vendor, std::uint16_t product, std::string_view name);

class WiiRemoteLayout {
  public:
    /// The remote alone, held sideways.
    WiiRemoteLayout() = default;
    /// The remote with `extension` (a Nunchuk, held upright, or a Classic Controller) whose
    /// device reports `reports`.
    WiiRemoteLayout(WiiRemotePart extension, const PadCapabilities& reports);

    /// What the pad reports, as the translator reads it.
    [[nodiscard]] PadCapabilities capabilities() const;
    /// `in` from `part` in gamepad codes; nothing for an input the pad has no place for.
    [[nodiscard]] std::optional<PadEvent> map(WiiRemotePart part, const PadEvent& in) const;

    /// The extension it is held with, or nothing alone.
    [[nodiscard]] std::optional<WiiRemotePart> extension() const noexcept {
        return extension_;
    }

  private:
    std::optional<WiiRemotePart> extension_;
    /// The extension's ranges, by its own axis codes.
    std::map<std::uint16_t, AxisRange> ranges_;
};

/// Where a remote points: on the screen, from 0 to 1 across and down, or off it. While its range is
/// the camera's whole view (the default), anywhere the camera sees the bar is on the screen and
/// the place is the camera's aim.
struct WiiAim {
    bool onScreen{false};
    float x{};
    float y{};

    friend bool operator==(const WiiAim&, const WiiAim&) = default;
};

/// Where a remote points, from its camera's dots: up to four on ABS_HAT0X to ABS_HAT3Y, 0 to 1023
/// across and 0 to 767 down, 1023 where there is none. The sensor bar's two ends are two dots;
/// their middle, mirrored across, is the camera's aim, one dot alone standing for both; the
/// range says where the screen is in those aims, and an aim past its edges is off the screen.
class WiiPointer {
  public:
    /// Reads the screen as `range` from the next report on.
    void setRange(const WiiPointerRange& range) noexcept {
        range_ = range;
    }

    /// Takes one camera event; at the end of each report, where the remote points when that
    /// changed.
    [[nodiscard]] std::optional<WiiAim> feed(const PadEvent& in);
    /// The remote's A going `down` or up; true when it is the pointer's button: pressed while
    /// the remote points at the screen, and that press let go wherever it then points.
    [[nodiscard]] bool press(bool down);

  private:
    static constexpr std::int32_t noDot = 1023;
    /// x then y of each dot.
    std::array<std::int32_t, 8> dots_{noDot, noDot, noDot, noDot, noDot, noDot, noDot, noDot};
    WiiPointerRange range_;
    WiiAim last_;
    bool pressing_{false};
};

} // namespace opensu::gamepad

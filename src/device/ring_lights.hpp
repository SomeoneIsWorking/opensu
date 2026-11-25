// ring_lights — a handheld's stick ring lights, the kernel's multicolour LED named
// `<device>:rgb:joystick_rings` (the ROG Ally, Legion Go and others): one colour for every zone and
// a brightness, written to its sysfs files. Absent where the kernel exposes no such LED. Where the
// LED class caps a packed zone below 0xFFFFFF (the Ally's driver: 255, blue alone) the files carry
// no colour, only off and on, and `colours()` says so.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace opensu::device {

/// Where the kernel lists its LEDs on Linux.
inline constexpr std::string_view ledsDir{"/sys/class/leds"};

struct RingColour {
    std::uint8_t red{255};
    std::uint8_t green{255};
    std::uint8_t blue{255};

    friend bool operator==(const RingColour&, const RingColour&) = default;
};

class RingLights {
  public:
    /// The first `*:rgb:joystick_rings` LED under `leds` whose colour layout is known, else none.
    [[nodiscard]] static std::optional<RingLights> find(
        const std::filesystem::path& leds = std::filesystem::path{ledsDir});

    /// Every zone `colour` at `percent` of full brightness; 0 turns them off. Empty when
    /// written, else why not.
    [[nodiscard]] std::string set(RingColour colour, int percent) const;

    /// Whether the files can carry any colour; else only off and full on work through them.
    [[nodiscard]] bool colours() const noexcept {
        return colours_;
    }

    /// How many zones the lights have.
    [[nodiscard]] std::size_t zones() const noexcept {
        return zones_;
    }

  private:
    RingLights(std::filesystem::path dir, std::size_t zones, bool packed, int maxBrightness,
               bool colours)
        : dir_{std::move(dir)}, zones_{zones}, packed_{packed}, maxBrightness_{maxBrightness},
          colours_{colours} {
    }

    std::filesystem::path dir_;
    std::size_t zones_;
    /// Each zone one `rgb` value, 0xRRGGBB (the Ally); else one `red green blue` zone.
    bool packed_;
    int maxBrightness_;
    bool colours_;
};

} // namespace opensu::device

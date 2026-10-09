// brightness_control — the display brightness the quick menu and the Devices page change. Wraps the
// backlight, which a machine may not have; without one there is no brightness row anywhere.
#pragma once

#include <optional>
#include <string>

#include "host/backlight.hpp"

namespace opensu::app {

/// How much one brightness step moves.
inline constexpr int brightnessStep = 5;

class BrightnessControl {
  public:
    /// `backlight` may be null; it outlives the control.
    explicit BrightnessControl(host::Backlight* backlight) noexcept : backlight_{backlight} {
    }

    [[nodiscard]] bool available() const noexcept {
        return backlight_ != nullptr;
    }
    /// The brightness now, 0 to 100, or nothing without a backlight or when it cannot be read.
    [[nodiscard]] std::optional<int> percent() const;
    /// Sets the brightness; empty when done, else why not.
    std::string set(int percent);
    /// Moves the brightness by `delta` percent.
    std::string step(int delta);

  private:
    host::Backlight* backlight_;
};

} // namespace opensu::app

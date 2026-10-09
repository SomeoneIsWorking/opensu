// volume_osd — the volume display that appears when the system volume changes: shown on every
// change, held for a moment, then faded out. iiSU has no volume overlay (it hands the volume keys
// to Android), so this is opensu's own; it fades like iiSU's panels do (motion.md §2.5).
#pragma once

#include <chrono>

#include "panel_fade.hpp"

namespace opensu::ui {

/// How long the display stays up after the last change.
inline constexpr std::chrono::milliseconds volumeOsdHold{1400};

struct VolumeLevel {
    /// 0 to 100.
    int percent{0};
    bool muted{false};

    bool operator==(const VolumeLevel&) const = default;
};

class VolumeOsd {
  public:
    using Clock = std::chrono::steady_clock;

    /// Shows `level` now and restarts the hold.
    void show(const VolumeLevel& level, Clock::time_point now) noexcept;
    /// Starts the fade-out once the hold is over.
    void tick(Clock::time_point now) noexcept;

    [[nodiscard]] const VolumeLevel& level() const noexcept {
        return level_;
    }
    [[nodiscard]] PanelLook look(Clock::time_point now) const noexcept {
        return fade_.look(now);
    }
    /// Whether anything is drawn: shown, or still fading out.
    [[nodiscard]] bool visible(Clock::time_point now) const noexcept {
        return fade_.visible(now);
    }
    /// Whether it is up and not on its way out.
    [[nodiscard]] bool holding() const noexcept {
        return holding_;
    }

  private:
    PanelFade fade_{FadeSpec{110.0f, 260.0f, 0.96f, 140.0f, 0.985f}};
    VolumeLevel level_;
    Clock::time_point shownAt_{};
    bool holding_{false};
};

} // namespace opensu::ui

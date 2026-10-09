// panel_fade — the fade and scale a panel shows and hides with. iiSU's global search fades 120 ms
// and grows from 0.96 over 140 ms (motion.md §2.5); its context menu fades in 110 ms growing from
// 0.96 and out in 95 ms shrinking to 0.985 (§2.2).
#pragma once

#include <chrono>

namespace opensu::ui {

struct FadeSpec {
    /// Alpha 0 to 1 and back, in milliseconds.
    float fadeInMs{};
    float fadeOutMs{};
    /// The scale a panel appears at, reached over `scaleInMs`, and the one it leaves toward.
    float scaleFrom{1.0f};
    float scaleInMs{};
    float scaleTo{1.0f};
};

/// How a panel is drawn at an instant.
struct PanelLook {
    float alpha{1.0f};
    float scale{1.0f};
};

class PanelFade {
  public:
    using Clock = std::chrono::steady_clock;

    explicit PanelFade(const FadeSpec& spec) noexcept : spec_{spec} {
    }

    void show(Clock::time_point now) noexcept;
    void hide(Clock::time_point now) noexcept;
    /// Shows or hides the panel when `open` is not what it is now.
    void follow(bool open, Clock::time_point now) noexcept;

    /// How opaque the panel is at `now`: 0 once it has faded out.
    [[nodiscard]] float alpha(Clock::time_point now) const noexcept;
    /// The scale it is drawn at.
    [[nodiscard]] float scale(Clock::time_point now) const noexcept;
    [[nodiscard]] PanelLook look(Clock::time_point now) const noexcept {
        return PanelLook{alpha(now), scale(now)};
    }
    /// Whether anything is drawn: shown, or still fading out.
    [[nodiscard]] bool visible(Clock::time_point now) const noexcept;

  private:
    [[nodiscard]] float sinceMs(Clock::time_point now) const noexcept;

    FadeSpec spec_;
    Clock::time_point changedAt_{};
    /// The alpha and the scale when the last change came, which the next run starts from.
    float alphaAtChange_{0.0f};
    float scaleAtChange_{1.0f};
    bool shown_{false};
};

} // namespace opensu::ui

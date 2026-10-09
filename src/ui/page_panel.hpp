// page_panel — a full-screen page of categories and rows (the Settings screen, the Devices page)
// with its fade, its painter call and the pointer hit-test of its categories and rows.
#pragma once

#include <chrono>

#include "raylib.h"

#include "panel_fade.hpp"
#include "pointer_target.hpp"
#include "settings_page.hpp"

namespace opensu::ui {

class PagePanel {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] SettingsPage& page() noexcept {
        return page_;
    }
    [[nodiscard]] const SettingsPage& page() const noexcept {
        return page_;
    }
    [[nodiscard]] bool isOpen() const noexcept {
        return page_.isOpen();
    }
    /// Whether the page is drawn, which includes fading out.
    [[nodiscard]] bool visible(Clock::time_point now) const noexcept {
        return fade_.visible(now);
    }

    /// Starts the fade the page's open state calls for.
    void tick(Clock::time_point now) noexcept {
        fade_.follow(page_.isOpen(), now);
    }

    /// Draws the page between `topInset` and `bottomInset` in a `size` frame at `dp` pixels per dp.
    void draw(Vector2 size, float dp, float topInset, float bottomInset, Clock::time_point now) const;

    /// The category, row or slider level under `point`, or nothing.
    [[nodiscard]] PointerTarget pointAt(Vector2 point, Vector2 size, float dp, float topInset,
                                        float bottomInset) const;
    /// Moves the page's focus to `target`, when it is one of its elements; reports whether it moved.
    bool focusTarget(const PointerTarget& target);

  private:
    SettingsPage page_;
    PanelFade fade_{FadeSpec{300.0f, 300.0f, 1.0f, 0.0f, 1.0f}};
};

} // namespace opensu::ui

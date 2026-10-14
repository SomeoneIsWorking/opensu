// downloads_panel — the Downloads page with its fade, its painter call and the pointer hit-test of
// its rows and buttons, as `PagePanel` is for the Settings-style pages.
#pragma once

#include <chrono>

#include "paint.hpp"

#include "downloads_page.hpp"
#include "downloads_page_painter.hpp"
#include "panel_fade.hpp"
#include "pointer_target.hpp"

namespace opensu::ui {

class DownloadsPanel {
  public:
    using Clock = std::chrono::steady_clock;

    explicit DownloadsPanel(Typeface& typeface) noexcept : painter_{typeface} {
    }

    [[nodiscard]] DownloadsPage& page() noexcept {
        return page_;
    }
    [[nodiscard]] const DownloadsPage& page() const noexcept {
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
    void draw(Canvas& canvas, Point size, float dp, float topInset, float bottomInset,
              Clock::time_point now) const;

    /// The row or button under `point`, or nothing.
    [[nodiscard]] PointerTarget pointAt(Point point, Point size, float dp, float topInset,
                                        float bottomInset) const;
    /// Moves the page's focus to `target`, when it is one of its elements; reports whether it
    /// moved.
    bool focusTarget(const PointerTarget& target);

  private:
    DownloadsPage page_;
    DownloadsPagePainter painter_;
    PanelFade fade_{FadeSpec{300.0f, 300.0f, 1.0f, 0.0f, 1.0f}};
};

} // namespace opensu::ui

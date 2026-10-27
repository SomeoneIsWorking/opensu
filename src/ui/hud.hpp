// hud — the chrome drawn around the home grid: ground, top bar, corner hints, toast.
//
// The top bar is iiSU's single-screen row (home-grid.md §2.2): the centre title pill (empty on
// Home, the console's name inside one) and the status pill, which holds opensu's launchers'
// badges where iiSU has its bell.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "paint.hpp"

#include "breadcrumb_painter.hpp"
#include "button_glyph.hpp"
#include "corner_hints.hpp"
#include "device/battery.hpp"
#include "glass.hpp"
#include "gpu2d/ui/typesetter.hpp"
#include "launcher_badges.hpp"
#include "library/game.hpp"
#include "status_pill.hpp"
#include "top_bar_layout.hpp"
#include "top_bar_metrics.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// The HUD palette.
namespace palette {
inline constexpr Colour ground{0xf4, 0xf3, 0xf7, 255};
inline constexpr Colour panel{0xff, 0xff, 0xff, 255};
inline constexpr Colour ink{0x2b, 0x27, 0x33, 255};
inline constexpr Colour inkSoft{0x6f, 0x68, 0x80, 255};
/// The focus outline, a switch's "on" and a slider's fill.
inline constexpr Colour control{0x4D, 0x46, 0x55, 255};
/// A chosen entry, the lit part of a focused row.
inline constexpr Colour chosen{0xE6, 0xE4, 0xEE, 255};
/// A switch's "off" and a slider's track.
inline constexpr Colour off{0xB7, 0xB2, 0xC0, 255};
} // namespace palette

/// Whether the home screen's chrome is dark for the palette's ground (iiSU ya0.e).
[[nodiscard]] bool chromeDark() noexcept;

class Hud {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long a toast stays up.
    static constexpr std::chrono::milliseconds toastLifetime{4000};

    /// `device` is which prompts the corner hints draw.
    Hud(const input::Prompts& prompts, gpu2d::ui::Typesetter& text, IconAtlas& icons) noexcept
        : text_{text}, statusPill_{text, icons}, crumbs_{text}, glyphs_{prompts, text} {
    }

    /// The window in pixels and its pixels per dp.
    struct Size {
        int width{};
        int height{};
        float dp{1.0f};
    };
    void setSize(const Size& size) noexcept;

    /// Height the top bar takes from the grid, in pixels (iiSU dl3.i).
    [[nodiscard]] float topInset() const noexcept;
    /// Height the bottom chrome takes from the grid, in pixels (iiSU dl3.j).
    [[nodiscard]] float bottomInset() const noexcept;

    void setStatus(std::string text) {
        status_ = std::move(text);
    }
    void setClock(std::string text) {
        clock_ = std::move(text);
    }
    void setBattery(std::optional<device::BatteryStatus> battery) noexcept {
        battery_ = battery;
    }
    void setLaunchers(std::vector<LauncherBadge> launchers) {
        launchers_ = std::move(launchers);
    }
    /// The centre title pill's text; empty draws no pill.
    void setTitle(std::string title) {
        title_ = std::move(title);
    }
    /// The breadcrumb trail; an empty trail draws nothing.
    void setTrail(Trail trail) {
        trail_ = std::move(trail);
    }
    /// Each level of the trail where the top bar draws it.
    [[nodiscard]] std::vector<Rect> crumbCells() const;
    /// What each button does now; the corners name only the ones that do something.
    void setHints(const HintContext& hints) noexcept {
        hints_ = hints;
    }
    /// A launcher's badge where the top bar draws it, with the room a pointer takes it in.
    struct BadgeCell {
        LauncherBadge badge;
        Rect hit;
    };
    /// The badges the top bar shows, in order.
    [[nodiscard]] std::vector<BadgeCell> badgeCells() const;
    void setToast(std::string text, bool isError, Clock::time_point now);
    /// Clears a toast whose time is up.
    void tick(Clock::time_point now);

    [[nodiscard]] const std::string& status() const noexcept {
        return status_;
    }
    [[nodiscard]] const std::string& toast() const noexcept {
        return toast_;
    }
    [[nodiscard]] bool toastIsError() const noexcept {
        return toastError_;
    }

    /// What the pointer rests on in the top bar: a level of the trail, a launcher's badge.
    struct TopBarHover {
        std::optional<std::size_t> crumb;
        std::optional<library::Source> launcher;
    };

    void drawGround(Canvas& canvas) const;
    void drawTopBar(Canvas& canvas, const TopBarHover& hover);
    void drawHints(Canvas& canvas) const;
    /// The toast, centred low on the screen and kept above `floor`, the top of what the shell
    /// draws along the bottom edge (the dock).
    void drawToast(Canvas& canvas, float floor) const;

  private:
    /// One hundredth of the window's shorter side, the unit of opensu's own chrome.
    [[nodiscard]] float unit() const noexcept;
    [[nodiscard]] TopBarMetrics metrics() const noexcept;
    void drawTitlePill(Canvas& canvas, float top) const;
    /// The title pill's body in a row whose top is `top`.
    [[nodiscard]] Rect titlePillBody(float top) const;
    /// Where the trail may stand: from the row's start to the title pill or the status pill.
    [[nodiscard]] BreadcrumbFrame crumbFrame() const;
    /// One corner prompt panel of (glyph, label) entries (iiSU jj2.b).
    void drawPromptPanel(Canvas& canvas, const HintPanelMetrics& panel,
                         const std::vector<Prompt>& prompts, bool atEnd) const;
    [[nodiscard]] TopBarLayout topBarLayout() const;

    int width_{};
    int height_{};
    float dp_{1.0f};
    std::string status_;
    std::string clock_;
    std::optional<device::BatteryStatus> battery_;
    std::vector<LauncherBadge> launchers_;
    std::string title_;
    Trail trail_;
    HintContext hints_;
    Clock::time_point now_{};
    std::string toast_;
    bool toastError_{false};
    Clock::time_point toastUntil_{};
    gpu2d::ui::Typesetter& text_;
    StatusPillPainter statusPill_;
    GlassPainter glass_;
    BreadcrumbPainter crumbs_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui

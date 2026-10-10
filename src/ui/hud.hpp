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

#include "raylib.h"

#include "breadcrumb_painter.hpp"
#include "button_glyph.hpp"
#include "corner_hints.hpp"
#include "device/battery.hpp"
#include "glass.hpp"
#include "launcher_badges.hpp"
#include "library/game.hpp"
#include "status_pill.hpp"
#include "top_bar_layout.hpp"
#include "top_bar_metrics.hpp"
#include "typeface.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

/// The HUD palette, as 8-bit-per-channel colours.
struct Colour {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    [[nodiscard]] operator Color() const noexcept {
        return Color{r, g, b, a};
    }
};

namespace palette {
inline constexpr Colour ground{0xf4, 0xf3, 0xf7, 255};
inline constexpr Colour panel{0xff, 0xff, 0xff, 255};
inline constexpr Colour ink{0x2b, 0x27, 0x33, 255};
inline constexpr Colour inkSoft{0x6f, 0x68, 0x80, 255};
} // namespace palette

class Hud {
  public:
    using Clock = std::chrono::steady_clock;

    /// How long a toast stays up.
    static constexpr std::chrono::milliseconds toastLifetime{4000};

    /// `device` is which prompts the corner hints draw.
    Hud(const input::Prompts& prompts, Typeface& typeface, IconAtlas& icons) noexcept
        : typeface_{typeface}, statusPill_{typeface, icons}, crumbs_{typeface},
          glyphs_{prompts, typeface} {
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
    /// The breadcrumb trail and the level the pointer is on; an empty trail draws nothing.
    void setTrail(Trail trail) {
        trail_ = std::move(trail);
    }
    void setCrumbHover(std::optional<std::size_t> crumb) noexcept {
        crumbHover_ = crumb;
    }
    /// The level of the trail under the point, from the geometry the trail paints at.
    [[nodiscard]] std::optional<std::size_t> crumbAt(float x, float y) const;
    /// What each button does now; the corners name only the ones that do something.
    void setHints(const HintContext& hints) noexcept {
        hints_ = hints;
    }
    /// The launcher the pointer is on, which its badge lights for.
    void setLauncherHover(std::optional<library::Source> source) noexcept {
        launcherHover_ = source;
    }
    /// The launcher whose badge is under the point, from the geometry the top bar paints at.
    [[nodiscard]] std::optional<library::Source> launcherAt(float x, float y) const;
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

    void drawGround() const;
    void drawTopBar();
    void drawHints() const;
    void drawToast() const;

  private:
    /// One hundredth of the window's shorter side, the unit of opensu's own chrome.
    [[nodiscard]] float unit() const noexcept;
    [[nodiscard]] TopBarMetrics metrics() const noexcept;
    void drawTitlePill(float top) const;
    /// The title pill's body in a row whose top is `top`.
    [[nodiscard]] Rect titlePillBody(float top) const;
    /// Where the trail may stand: from the row's start to the title pill or the status pill.
    [[nodiscard]] BreadcrumbFrame crumbFrame() const;
    /// One corner prompt panel of (glyph, label) entries (iiSU jj2.b).
    void drawPromptPanel(const HintPanelMetrics& panel, const std::vector<Prompt>& prompts,
                         bool atEnd) const;
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
    std::optional<std::size_t> crumbHover_;
    HintContext hints_;
    std::optional<library::Source> launcherHover_;
    Clock::time_point now_{};
    std::string toast_;
    bool toastError_{false};
    Clock::time_point toastUntil_{};
    Typeface& typeface_;
    StatusPillPainter statusPill_;
    GlassPainter glass_;
    BreadcrumbPainter crumbs_;
    ButtonGlyphPainter glyphs_;
};

} // namespace opensu::ui

// hud — the chrome drawn around the home grid: ground, top bar, corner hints, toast.
//
// The top bar is iiSU's single-screen row (home-grid.md §2.2): the friends slot, which
// iideck fills with its launchers' badges, the centre title pill (empty on Home, the
// console's name inside one) and the status pill.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"

#include "button_glyph.hpp"
#include "device/battery.hpp"
#include "glass.hpp"
#include "launcher_badges.hpp"
#include "status_pill.hpp"
#include "top_bar_metrics.hpp"

namespace iideck::ui {

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

    /// The window in pixels and its pixels per dp.
    void setSize(int width, int height, float dp) noexcept;

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
    /// One hundredth of the window's shorter side, the unit of iideck's own chrome.
    [[nodiscard]] float unit() const noexcept;
    [[nodiscard]] TopBarMetrics metrics() const noexcept;
    void drawTitlePill(float top) const;
    /// One corner prompt panel of (glyph, label) entries (iiSU jj2.b).
    void drawPromptPanel(const HintPanelMetrics& panel,
                         std::span<const std::pair<const char*, const char*>> prompts,
                         bool atEnd) const;

    int width_{};
    int height_{};
    float dp_{1.0f};
    std::string status_;
    std::string clock_;
    std::optional<device::BatteryStatus> battery_;
    std::vector<LauncherBadge> launchers_;
    std::string title_;
    Clock::time_point now_{};
    std::string toast_;
    bool toastError_{false};
    Clock::time_point toastUntil_{};
    StatusPillPainter statusPill_;
    GlassPainter glass_;
    ButtonGlyphPainter glyphs_;
    LauncherBadgePainter badges_;
};

} // namespace iideck::ui

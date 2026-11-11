// recommended_scale — the pixels per dp a screen reads best at where no window system gives a
// scale: KWin's rule (`OutputConfigurationStore::chooseScale`) in Windows' 25% steps.
#pragma once

#include <algorithm>
#include <cmath>

#include "letterbox.hpp"

namespace opensu::display {

/// A screen's glass in millimetres, as its EDID states it; zero when unknown.
struct Panel {
    int widthMm{0};
    int heightMm{0};
    /// Built into the device (eDP, LVDS, DSI): held or on a lap.
    bool builtIn{false};

    /// KWin's test: under 3 mm a side is a screen that misreports its size.
    [[nodiscard]] bool known() const noexcept {
        return widthMm >= 3 && heightMm >= 3;
    }
};

/// What a kind of screen is read at: the pixels per inch one dp per pixel suits, and the dp it
/// keeps along its long and short sides.
struct ScaleTarget {
    float dpi;
    float longDp;
    float shortDp;
};

/// KWin's desk monitor: 96 dpi, 800 dp a side.
inline constexpr ScaleTarget monitorTarget{.dpi = 96.0F, .longDp = 800.0F, .shortDp = 800.0F};
/// KWin's tablet dpi for a built-in panel, keeping the 853 x 480 dp iiSU lays a handheld out for.
inline constexpr ScaleTarget builtInTarget{.dpi = 130.0F, .longDp = 853.0F, .shortDp = 480.0F};
/// KWin takes a screen taller than this for a TV.
inline constexpr int tvShortSideMm = 500;
/// A TV's short side in dp: KWin's 30.5 dpi on a 58-inch screen, 2.5 at 4K. A TV's EDID size is
/// often a placeholder and a larger one is watched from farther, so its size is left out.
inline constexpr float tvShortSideDp = 864.0F;
inline constexpr float mostScale = 3.0F;
inline constexpr float scaleStep = 0.25F;
/// KWin leaves a screen that would scale by less than this at 1.
inline constexpr float leastScaled = 1.2F;

/// The scale for a screen of `pixels` whose glass is `panel`: a TV's, also taken when the size
/// is unknown, a desk monitor's or a built-in panel's, to the nearest `scaleStep`, from 1 to
/// `mostScale`.
[[nodiscard]] inline float recommendedScale(Size pixels, Panel panel) noexcept {
    const float longPx = static_cast<float>(std::max(pixels.width, pixels.height));
    const float shortPx = static_cast<float>(std::min(pixels.width, pixels.height));
    if (shortPx <= 0.0F) {
        return 1.0F;
    }
    const float shortMm = static_cast<float>(std::min(panel.widthMm, panel.heightMm));
    const float longMm = static_cast<float>(std::max(panel.widthMm, panel.heightMm));
    float scale = shortPx / tvShortSideDp;
    if (panel.known() && (panel.builtIn || shortMm <= static_cast<float>(tvShortSideMm))) {
        const ScaleTarget target = panel.builtIn ? builtInTarget : monitorTarget;
        const auto along = [&target](float px, float mm, float leastDp) {
            const float dpi = px / (mm / 25.4F);
            return std::clamp(dpi / target.dpi, 1.0F, std::max(px / leastDp, 1.0F));
        };
        scale =
            std::min(along(longPx, longMm, target.longDp), along(shortPx, shortMm, target.shortDp));
    }
    if (scale < leastScaled) {
        return 1.0F;
    }
    return std::min(std::round(scale / scaleStep) * scaleStep, mostScale);
}

} // namespace opensu::display

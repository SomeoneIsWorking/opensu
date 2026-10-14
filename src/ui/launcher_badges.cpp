#include "launcher_badges.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "progress_spinner.hpp"

namespace opensu::ui {
namespace {

// The dock item's hover fill: the ink at 12 %.
constexpr Colour hoverFill{0x2B, 0x27, 0x33, 31};
constexpr Colour logo{0x2B, 0x27, 0x33, 255};
constexpr Colour dotReady{0x3D, 0xDC, 0x84, 255};
constexpr Colour dotWorking{0xF2, 0xB1, 0x34, 255};
constexpr Colour dotFailed{0xE0, 0x4F, 0x5F, 255};
// iiSU xe4.h's blue, the focus ring's.
constexpr Colour progressInk{0x2C, 0x88, 0xFF, 255};
constexpr Colour progressTrack{0x2B, 0x27, 0x33, 28};

Colour dotFor(ServiceState state) noexcept {
    if (state == ServiceState::Starting) {
        return dotWorking;
    }
    if (state == ServiceState::Failed || state == ServiceState::Blocked) {
        return dotFailed;
    }
    return dotReady;
}

} // namespace

void LauncherBadgePainter::paint(Canvas& canvas, std::span<const LauncherBadge> badges,
                                 const BadgeRow& row) {
    std::size_t cell = 0;
    for (const LauncherBadge& badge : badges) {
        if (badge.state == ServiceState::Hidden || cell >= row.cells.size()) {
            continue;
        }
        paintOne(canvas, badge, row, row.cells[cell++]);
    }
}

void LauncherBadgePainter::paintOne(Canvas& canvas, const LauncherBadge& badge, const BadgeRow& row,
                                    const Rect& cell) {
    const Point centre{cell.centreX(), cell.centreY()};
    const float diameter = cell.width;
    const double seconds = row.seconds;
    const float radius = diameter * 0.5f;
    const float line = std::max(diameter * 0.07f, 1.5f);
    // The ring sits just outside the circle, where iiSU's bell carries its spinner.
    const float ringRadius = radius + line * 1.4f;
    if (badge.progress) {
        const float sweep = static_cast<float>(std::clamp(*badge.progress, 0.0, 1.0)) * 360.0f;
        const float inner = ringRadius - line * 0.5f;
        const float outer = ringRadius + line * 0.5f;
        canvas.ring(gpu2d::Ring{.centre = centre,
                                .inner = inner,
                                .outer = outer,
                                .startDegrees = 0.0f,
                                .endDegrees = 360.0f},
                    progressTrack);
        canvas.ring(gpu2d::Ring{.centre = centre,
                                .inner = inner,
                                .outer = outer,
                                .startDegrees = -90.0f,
                                .endDegrees = -90.0f + sweep},
                    progressInk);
    } else if (badge.state == ServiceState::Starting) {
        drawSpinner(canvas, centre, ringRadius * 2.0f, line, dotWorking, seconds);
    }

    if (row.hovered == badge.source) {
        canvas.fillCircle(centre, ringRadius + line, hoverFill);
    }

    const bool usable = badge.state == ServiceState::Ready || badge.state == ServiceState::Starting;
    const int pixels = std::max(static_cast<int>(std::lround(diameter * 0.9f)), 1);
    if (const gpu2d::Texture* mark = icons_.mask(badge.icon, pixels)) {
        const Colour tint{logo.r, logo.g, logo.b, static_cast<std::uint8_t>(usable ? 255 : 110)};
        const auto side = static_cast<float>(pixels);
        canvas.image(mark->view(), wholeOf(*mark),
                     gpu2d::Rect{std::round(centre.x - side * 0.5f),
                                 std::round(centre.y - side * 0.5f), side, side},
                     tint);
    }

    // A presence dot on the logo's lower right, cut out of it by a light rim.
    const float dot = diameter * 0.2f;
    const float along = radius * std::numbers::sqrt2_v<float> * 0.62f;
    const Point at{centre.x + along, centre.y + along};
    canvas.fillCircle(at, dot + line * 0.8f, Colour{0xFF, 0xFF, 0xFF, 235});
    canvas.fillCircle(at, dot, dotFor(badge.state));
}

} // namespace opensu::ui

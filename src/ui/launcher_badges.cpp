#include "launcher_badges.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "progress_spinner.hpp"

namespace iideck::ui {
namespace {

constexpr Color face{0xFF, 0xFF, 0xFF, 235};
constexpr Color rim{0x2B, 0x27, 0x33, 30};
constexpr Color logo{0x2B, 0x27, 0x33, 255};
constexpr Color dotReady{0x3D, 0xDC, 0x84, 255};
constexpr Color dotWorking{0xF2, 0xB1, 0x34, 255};
constexpr Color dotFailed{0xE0, 0x4F, 0x5F, 255};
// iiSU xe4.h's blue, the focus ring's.
constexpr Color progressInk{0x2C, 0x88, 0xFF, 255};
constexpr Color progressTrack{0x2B, 0x27, 0x33, 28};

Color dotFor(ServiceState state) noexcept {
    if (state == ServiceState::Starting) {
        return dotWorking;
    }
    if (state == ServiceState::Failed || state == ServiceState::Blocked) {
        return dotFailed;
    }
    return dotReady;
}

} // namespace

void LauncherBadgePainter::paint(std::span<const LauncherBadge> badges, const BadgeRow& row) {
    float x = row.start.x;
    for (const LauncherBadge& badge : badges) {
        if (badge.state == ServiceState::Hidden) {
            continue;
        }
        paintOne(badge, row, Vector2{x + row.diameter * 0.5f, row.start.y});
        x += row.diameter + row.gap;
    }
}

void LauncherBadgePainter::paintOne(const LauncherBadge& badge, const BadgeRow& row,
                                    Vector2 centre) {
    const float diameter = row.diameter;
    const double seconds = row.seconds;
    const float radius = diameter * 0.5f;
    const float line = std::max(diameter * 0.07f, 1.5f);
    // The ring sits just outside the circle, where iiSU's bell carries its spinner.
    const float ringRadius = radius + line * 1.4f;
    if (badge.progress) {
        const float sweep = static_cast<float>(std::clamp(*badge.progress, 0.0, 1.0)) * 360.0f;
        DrawRing(centre, ringRadius - line * 0.5f, ringRadius + line * 0.5f, 0.0f, 360.0f, 64,
                 progressTrack);
        DrawRing(centre, ringRadius - line * 0.5f, ringRadius + line * 0.5f, -90.0f, -90.0f + sweep,
                 64, progressInk);
    } else if (badge.state == ServiceState::Starting) {
        drawSpinner(centre, ringRadius * 2.0f, line, dotWorking, seconds);
    }

    DrawCircleV(centre, radius, face);
    DrawRing(centre, radius - 1.0f, radius, 0.0f, 360.0f, 64, rim);

    const bool usable = badge.state == ServiceState::Ready || badge.state == ServiceState::Starting;
    const int pixels = std::max(static_cast<int>(std::lround(diameter * 0.56f)), 1);
    if (const Texture* mark = icons_.mask(badge.icon, pixels)) {
        const Color tint{logo.r, logo.g, logo.b, static_cast<unsigned char>(usable ? 255 : 110)};
        DrawTexture(
            *mark, static_cast<int>(std::lround(centre.x - static_cast<float>(pixels) * 0.5f)),
            static_cast<int>(std::lround(centre.y - static_cast<float>(pixels) * 0.5f)), tint);
    }

    // A presence dot on the circle's lower right, cut out of it by a white rim.
    const float dot = diameter * 0.16f;
    const float along = radius * std::numbers::sqrt2_v<float> * 0.5f;
    const Vector2 at{centre.x + along, centre.y + along};
    DrawCircleV(at, dot + line * 0.8f, WHITE);
    DrawCircleV(at, dot, dotFor(badge.state));
}

} // namespace iideck::ui

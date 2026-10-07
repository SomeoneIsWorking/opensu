#include "tile_painter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>

#include "rlgl.h"

#include "typeface.hpp"

namespace iideck::ui {
namespace {

// iiSU gh3.q builds the home ya0 with darkHeroScrim = true, so home chrome is always dark.
// iiSU tx2.a: k and j for dark chrome.
constexpr float chromeK = 0.92f;
constexpr float chromeJ = 0.82f;

// iiSU xe4.h: default focus ring stops, evenly spaced.
constexpr std::array<Color, 10> ringStops{
    Color{0x71, 0xE0, 0xFF, 255}, Color{0x20, 0xDE, 0xFF, 255}, Color{0x2C, 0x88, 0xFF, 255},
    Color{0x77, 0x47, 0xFF, 255}, Color{0xC5, 0x6E, 0xFF, 255}, Color{0xC5, 0x6E, 0xFF, 255},
    Color{0x77, 0x47, 0xFF, 255}, Color{0x2C, 0x88, 0xFF, 255}, Color{0x20, 0xDE, 0xFF, 255},
    Color{0x71, 0xE0, 0xFF, 255},
};
// iiSU nx2.j: the ring under the chrome is inset 1.7 frames, the one above 0.9.
constexpr float ringUnderInset = 1.7f;
constexpr float ringOverInset = 0.9f;

// iiSU nx2.u: no-art letter is #66FFFFFF, clamp(0.34 min, 34, 92) px.
constexpr Color fallbackInk{0xFF, 0xFF, 0xFF, 0x66};

struct Stop {
    float at;
    Color colour;
};

Color alphaColour(int r, int g, int b, float alpha) noexcept {
    return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                 static_cast<unsigned char>(b),
                 static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f)};
}

Color sample(std::span<const Stop> stops, float t) noexcept {
    const float c = std::clamp(t, 0.0f, 1.0f);
    for (std::size_t i = 1; i < stops.size(); ++i) {
        if (c <= stops[i].at) {
            const float span = std::max(stops[i].at - stops[i - 1].at, 1e-6f);
            return mix(stops[i - 1].colour, stops[i].colour, (c - stops[i - 1].at) / span);
        }
    }
    return stops.back().colour;
}

/// Position along the top-left to bottom-right diagonal of `rect`, 0 to 1.
float diagonal(const Rect& rect, Vector2 point) noexcept {
    return ((point.x - rect.x) + (point.y - rect.y)) / std::max(rect.width + rect.height, 1.0f);
}

Color unpack(std::uint32_t rgb) noexcept {
    return Color{static_cast<unsigned char>((rgb >> 16) & 0xffu),
                 static_cast<unsigned char>((rgb >> 8) & 0xffu),
                 static_cast<unsigned char>(rgb & 0xffu), 255};
}

/// Scales drawing about a point for the lifetime of the object.
class ScopedScale {
  public:
    ScopedScale(float x, float y, float scale) {
        rlPushMatrix();
        rlTranslatef(x, y, 0.0f);
        rlScalef(scale, scale, 1.0f);
        rlTranslatef(-x, -y, 0.0f);
    }
    ~ScopedScale() {
        rlPopMatrix();
    }
    ScopedScale(const ScopedScale&) = delete;
    ScopedScale& operator=(const ScopedScale&) = delete;
};

} // namespace

Rectangle coverSource(float width, float height, const Rect& target) noexcept {
    const float sourceWidth = std::max(width, 1.0f);
    const float sourceHeight = std::max(height, 1.0f);
    const float targetAspect = std::max(target.width, 1.0f) / std::max(target.height, 1.0f);
    const float sourceAspect = sourceWidth / sourceHeight;
    // iiSU e01.k: keep the full side the target's aspect allows, centred.
    const float cropWidth = sourceAspect > targetAspect ? sourceHeight * targetAspect : sourceWidth;
    const float cropHeight =
        sourceAspect > targetAspect ? sourceHeight : sourceWidth / targetAspect;
    return Rectangle{(sourceWidth - cropWidth) * 0.5f, (sourceHeight - cropHeight) * 0.5f,
                     cropWidth, cropHeight};
}

void TilePainter::paint(const TileVisual& tile) const {
    if (tile.alpha <= 0.0f) {
        return;
    }
    const TileGeometry geometry = tileGeometry(tile.rect, tile.cell);
    // iiSU nx2.j: the canvas is scaled about the tile centre.
    const ScopedScale scaled{tile.rect.centreX(), tile.rect.centreY(), tile.scale};
    paintShadow(geometry, tile.alpha);
    if (tile.focused) {
        paintRing(geometry, ringUnderInset, tile.ringDegrees, tile.alpha);
    }
    paintChrome(geometry, tile.focused, tile.alpha);
    if (!tile.placeholder) {
        paintContent(tile, geometry);
    }
    if (tile.focused) {
        paintRing(geometry, ringOverInset, tile.ringDegrees, tile.alpha);
    }
}

void TilePainter::paintShadow(const TileGeometry& geometry, float alpha) const {
    // iiSU tx2.b, dark values.
    const float stroke = geometry.outerStroke;
    const float blur = std::max(2.5f, stroke * 1.75f);
    const float spread = std::max(0.75f, 1.25f * stroke);
    const float offsetY = std::max(1.25f, 2.1f * stroke);
    const Color shadow = alphaColour(0, 0, 0, 0.16f * alpha);
    RoundRect shape{geometry.outer, geometry.outerRadius};
    shape = shape.grown(spread);
    shape.rect.y += offsetY;
    fillSoft(shape, blur, [shadow](Vector2, float) {
        return shadow;
    });
}

void TilePainter::paintRing(const TileGeometry& geometry, float inset, float degrees,
                            float alpha) const {
    // iiSU tw2.b: outer round rect to a rect inset frame x k at the content radius.
    const RoundRect outer{geometry.outer, geometry.outerRadius};
    const float by = geometry.frameWidth * inset;
    const RoundRect inner{Rect{geometry.outer.x + by, geometry.outer.y + by,
                               std::max(geometry.outer.width - by * 2.0f, 0.0f),
                               std::max(geometry.outer.height - by * 2.0f, 0.0f)},
                          geometry.contentRadius};
    const float cx = geometry.outer.centreX();
    const float cy = geometry.outer.centreY();
    const float turn = degrees * std::numbers::pi_v<float> / 180.0f;
    // A sweep gradient sampled per vertex; edges are cut fine enough to follow it.
    fillBand(
        outer, inner,
        [cx, cy, turn, alpha](Vector2 point, float) {
            const float twoPi = 2.0f * std::numbers::pi_v<float>;
            float angle = std::atan2(point.y - cy, point.x - cx) - turn;
            angle = std::fmod(std::fmod(angle, twoPi) + twoPi, twoPi);
            const float position = angle / twoPi * static_cast<float>(ringStops.size() - 1);
            const auto low = static_cast<std::size_t>(position);
            const std::size_t high = std::min(low + 1, ringStops.size() - 1);
            return withAlpha(
                mix(ringStops[low], ringStops[high], position - static_cast<float>(low)), alpha);
        },
        Tessellation{12, 24});
}

void TilePainter::paintChrome(const TileGeometry& geometry, bool focused, float alpha) const {
    // iiSU tx2.a ("Grid.gradientChrome").
    const Rect& outer = geometry.outer;
    const RoundRect clip{outer, geometry.outerRadius};
    const float frame = geometry.frameWidth;
    const float stroke = geometry.outerStroke;
    const RoundRect strokeRect{Rect{outer.x + stroke * 0.5f, outer.y + stroke * 0.5f,
                                    outer.width - stroke, outer.height - stroke},
                               std::max(geometry.outerRadius - 1.4f, 0.0f)};

    // 1. A diagonal white gradient, once blurred and once crisp.
    const std::array<Stop, 4> sheen{
        Stop{0.0f, alphaColour(255, 255, 255, 0.24f * chromeK)},
        Stop{0.3f, alphaColour(254, 238, 255, 0.11f * chromeK)},
        Stop{0.55f, alphaColour(254, 244, 255, 0.16f * chromeK)},
        Stop{1.0f, alphaColour(255, 255, 255, 0.20f * chromeK)},
    };
    const auto sheenAt = [&sheen, &outer, alpha](Vector2 point, float) {
        return withAlpha(sample(sheen, diagonal(outer, point)), alpha);
    };
    strokeSoft(strokeRect, std::clamp(1.02f * frame, 3.8f, 10.0f),
               std::clamp(0.72f * frame, 2.8f, 7.2f), clip, sheenAt);
    strokeSoft(strokeRect, std::clamp(1.18f * stroke, 1.2f, 2.9f), 0.0f, clip, sheenAt);

    // 2. Two radial glints, bottom-right and top-left, centres 4% in (iiSU tx2.java:191-193).
    const float reach = 0.9f * std::min(outer.width, outer.height);
    const std::array<Stop, 3> warm{
        Stop{0.0f, alphaColour(0x6E, 0xEC, 0xFF, 0.1f * chromeJ)},
        Stop{0.5f, alphaColour(0x75, 0xB7, 0xB5, 0.05f * chromeJ)},
        Stop{1.0f, alphaColour(0x53, 0xA6, 0xA0, 0.0f)},
    };
    const std::array<Stop, 3> cool{
        Stop{0.0f, alphaColour(0x67, 0x4F, 0xDD, 0.105f * chromeJ)},
        Stop{0.5f, alphaColour(0x67, 0x4F, 0xDD, 0.05f * chromeJ)},
        Stop{1.0f, alphaColour(0x67, 0x4F, 0xDD, 0.0f)},
    };
    const Vector2 bottomRight{outer.right() - outer.width * 0.04f,
                              outer.bottom() - outer.height * 0.04f};
    const Vector2 topLeft{outer.x + outer.width * 0.04f, outer.y + outer.height * 0.04f};
    const float glint = std::clamp(0.85f * stroke, 1.0f, 2.2f);
    for (const auto& [centre, stops] : {std::pair{bottomRight, std::span<const Stop>{warm}},
                                        std::pair{topLeft, std::span<const Stop>{cool}}}) {
        strokeSoft(
            strokeRect, glint, 0.0f, clip,
            [centre, stops, reach, alpha](Vector2 point, float) {
                const float distance = std::hypot(point.x - centre.x, point.y - centre.y);
                return withAlpha(sample(stops, distance / reach), alpha);
            },
            Tessellation{8, 8});
    }

    // 3. A soft white glow just outside the content rect.
    const float glowAlpha = std::min(0.14f * chromeK, 0.2f);
    const RoundRect glow =
        RoundRect{geometry.content, geometry.contentRadius}.grown(std::max(0.38f * frame, 1.6f));
    const Color glowColour = alphaColour(255, 255, 255, glowAlpha * alpha);
    strokeSoft(glow, std::clamp(frame * 0.72f, 3.4f, 8.2f), std::clamp(frame * 0.56f, 2.6f, 6.5f),
               clip, [glowColour](Vector2, float) {
                   return glowColour;
               });

    // 4. A crisp white edge; 5. a second, stronger one when focused.
    const Color edge = alphaColour(255, 255, 255, 0.34f * alpha);
    strokeSoft(strokeRect, std::clamp(stroke, 1.0f, 2.25f), 0.0f, clip, [edge](Vector2, float) {
        return edge;
    });
    if (focused) {
        const Color bright = alphaColour(255, 255, 255, 0.46f * alpha);
        strokeSoft(strokeRect, std::clamp(1.18f * stroke, 1.3f, 2.4f), 0.0f, clip,
                   [bright](Vector2, float) {
                       return bright;
                   });
    }
}

void TilePainter::paintContent(const TileVisual& tile, const TileGeometry& geometry) const {
    const Rect& content = geometry.content;
    const Color tint = withAlpha(WHITE, tile.alpha);
    if (tile.platform != nullptr) {
        // iiSU g24.e: art clipped at the larger art radius, then the platform frame over it.
        const FrameGeometry frame = frameGeometry(content);
        if (tile.art != nullptr) {
            drawTextureRound(RoundRect{content, frame.artRadius}, *tile.art,
                             coverSource(static_cast<float>(tile.art->width),
                                         static_cast<float>(tile.art->height), content),
                             tint);
        }
        paintFrame(content, *tile.platform, tile.alpha);
        return;
    }
    if (tile.art != nullptr) {
        // iiSU nx2.l: a cover crop into the content rect at the content radius.
        drawTextureRound(RoundRect{content, geometry.contentRadius}, *tile.art,
                         coverSource(static_cast<float>(tile.art->width),
                                     static_cast<float>(tile.art->height), content),
                         tint);
        return;
    }
    paintFallback(content, tile.title, tile.alpha);
}

void TilePainter::paintFrame(const Rect& rect, const Platform& platform, float alpha) const {
    // The border sprite drawn as its own shapes: a stroke and a top-left tab, both on the
    // sprite's diagonal gradient.
    const FrameGeometry frame = frameGeometry(rect);
    const Color from = unpack(platform.strokeFrom);
    const Color to = unpack(platform.strokeTo);
    const auto colour = [&rect, from, to, alpha](Vector2 point, float) {
        return withAlpha(mix(from, to, diagonal(rect, point)), alpha);
    };
    const RoundRect outer{rect, frame.outerRadius};
    fillBand(outer, outer.grown(-frame.stroke), colour);
    fillRoundRect(RoundRect{Rect{rect.x, rect.y, frame.tab, frame.tab}, frame.tabRadius}, colour);
    // Only the tab's inner corner is round: square its other two inner corners.
    const float corner = std::min(frame.tabRadius, frame.tab * 0.5f);
    const auto square = [&colour](const Rect& part) {
        fillRoundRect(RoundRect{part, 0.0f}, colour);
    };
    square(Rect{rect.x + frame.tab - corner, rect.y, corner, corner});
    square(Rect{rect.x, rect.y + frame.tab - corner, corner, corner});
}

void TilePainter::paintFallback(const Rect& content, std::string_view title, float alpha) const {
    // iiSU yy0.f: the first letter or digit, upper-cased.
    const auto found = std::ranges::find_if(title, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0;
    });
    if (found == title.end()) {
        return;
    }
    const std::string letter(1,
                             static_cast<char>(std::toupper(static_cast<unsigned char>(*found))));
    const int size =
        static_cast<int>(std::clamp(std::min(content.width, content.height) * 0.34f, 34.0f, 92.0f));
    const float width = type().measure(letter, size);
    type().draw(letter.c_str(), content.centreX() - width * 0.5f,
                content.centreY() - static_cast<float>(size) * 0.5f, size,
                withAlpha(fallbackInk, alpha));
}

} // namespace iideck::ui

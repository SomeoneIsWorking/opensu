#include "tile_painter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "rlgl.h"

#include "typeface.hpp"

namespace iideck::ui {

/// The constants iiSU's tx2.a and tx2.b switch on the dark flag.
struct ChromeVariant {
    float k;
    float j;
    float glowStroke;
    float glowBlur;
    float glowAlpha;
    float glowCap;
    float edge;
    float focusedEdge;
    float shadowBlur;
    float shadowAlpha;
};

namespace {

constexpr ChromeVariant darkChrome{0.92f, 0.82f, 0.72f, 0.56f, 0.14f,
                                   0.2f,  0.34f, 0.46f, 1.75f, 0.16f};
constexpr ChromeVariant lightChrome{1.65f, 1.0f,  0.82f, 0.64f, 0.18f,
                                    0.28f, 0.58f, 0.72f, 2.0f,  0.13f};

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

// A console with no platform colours of its own: iiSU's prompt ink, lightened.
constexpr Color consoleFrom{0x8A, 0x82, 0x9C, 255};
constexpr Color consoleTo{0x4D, 0x46, 0x55, 255};
constexpr Color consoleDarkInk{0x2B, 0x27, 0x33, 255};

/// A launcher's card gradient, from its brand's colours.
struct Brand {
    Icon icon;
    Color from;
    Color to;
};
constexpr std::array<Brand, 3> brands{
    Brand{Icon::Steam, Color{0x2A, 0x47, 0x5E, 255}, Color{0x1B, 0x28, 0x38, 255}},
    Brand{Icon::Epic, Color{0x3A, 0x3A, 0x3F, 255}, Color{0x16, 0x16, 0x1A, 255}},
    Brand{Icon::Gog, Color{0x86, 0x32, 0x8A, 255}, Color{0x4A, 0x1A, 0x4D, 255}},
};
// The combined library's card: a calm violet apart from the stores' colours.
constexpr Color libraryFrom{0x5B, 0x4F, 0x9A, 255};
constexpr Color libraryTo{0x2F, 0x28, 0x5E, 255};
// A store badge's disc, dark so a white logo reads over any cover.
constexpr Color badgeDisc{0x16, 0x13, 0x1E, 200};
constexpr float badgeLogo = 0.62f;
// A folder card's mark is a quarter of the card; the library glyph is four tiles of it.
constexpr float markSide = 0.26f;
constexpr float markGap = 0.05f;
constexpr float glyphTile = 0.44f;

struct Stop {
    float at;
    Color colour;
};

/// `text` as one line, or two broken at the last space that lets the first fit `room`.
std::vector<std::string_view> breakLines(std::string_view text, const TextStyle& style,
                                         float room) {
    if (type().measure(text, style) <= room) {
        return {text};
    }
    for (std::size_t space = text.rfind(' '); space != std::string_view::npos && space > 0;
         space = text.rfind(' ', space - 1)) {
        const std::string_view first = text.substr(0, space);
        if (type().measure(first, style) <= room || first.find(' ') == std::string_view::npos) {
            return {first, text.substr(space + 1)};
        }
    }
    return {text};
}

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

void TilePainter::paint(const TileVisual& tile) {
    if (tile.alpha <= 0.0f) {
        return;
    }
    const TileGeometry geometry = tileGeometry(tile.rect, tile.cell);
    // iiSU nx2.j: the canvas is scaled about the tile centre.
    const ScopedScale scaled{tile.rect.centreX(), tile.rect.centreY(), tile.scale};
    const ChromeVariant& variant = tile.dark ? darkChrome : lightChrome;
    paintShadow(geometry, variant, tile.alpha);
    if (tile.focused) {
        paintRing(geometry, ringUnderInset, tile.ringDegrees, tile.alpha);
    }
    paintChrome(geometry, variant, tile.focused, tile.alpha);
    if (!tile.placeholder) {
        paintContent(tile, geometry);
    }
    if (tile.focused) {
        paintRing(geometry, ringOverInset, tile.ringDegrees, tile.alpha);
    }
}

void TilePainter::paintShadow(const TileGeometry& geometry, const ChromeVariant& variant,
                              float alpha) const {
    // iiSU tx2.b.
    const float stroke = geometry.outerStroke;
    const float blur = std::max(2.5f, stroke * variant.shadowBlur);
    const float spread = std::max(0.75f, 1.25f * stroke);
    const float offsetY = std::max(1.25f, 2.1f * stroke);
    const Color shadow = alphaColour(0, 0, 0, variant.shadowAlpha * alpha);
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

void TilePainter::paintChrome(const TileGeometry& geometry, const ChromeVariant& variant,
                              bool focused, float alpha) const {
    const float chromeK = variant.k;
    const float chromeJ = variant.j;
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
    const float glowAlpha = std::min(variant.glowAlpha * chromeK, variant.glowCap);
    const RoundRect glow =
        RoundRect{geometry.content, geometry.contentRadius}.grown(std::max(0.38f * frame, 1.6f));
    const Color glowColour = alphaColour(255, 255, 255, glowAlpha * alpha);
    strokeSoft(glow, std::clamp(frame * variant.glowStroke, 3.4f, 8.2f),
               std::clamp(frame * variant.glowBlur, 2.6f, 6.5f), clip,
               [glowColour](Vector2, float) {
                   return glowColour;
               });

    // 4. A crisp white edge; 5. a second, stronger one when focused.
    const Color edge = alphaColour(255, 255, 255, variant.edge * alpha);
    strokeSoft(strokeRect, std::clamp(stroke, 1.0f, 2.25f), 0.0f, clip, [edge](Vector2, float) {
        return edge;
    });
    if (focused) {
        const Color bright = alphaColour(255, 255, 255, variant.focusedEdge * alpha);
        strokeSoft(strokeRect, std::clamp(1.18f * stroke, 1.3f, 2.4f), 0.0f, clip,
                   [bright](Vector2, float) {
                       return bright;
                   });
    }
}

void TilePainter::paintContent(const TileVisual& tile, const TileGeometry& geometry) {
    const Rect& content = geometry.content;
    const Color tint = withAlpha(WHITE, tile.alpha);
    if (tile.kind == TileKind::Console && tile.art != nullptr) {
        // The console's card is the whole tile: it carries the glyph and its own frame.
        drawTextureRound(RoundRect{content, geometry.contentRadius}, *tile.art,
                         coverSource(static_cast<float>(tile.art->width),
                                     static_cast<float>(tile.art->height), content),
                         tint);
        return;
    }
    if (tile.kind == TileKind::Console) {
        const Color from = tile.platform != nullptr ? unpack(tile.platform->strokeFrom) : consoleFrom;
        const Color to = tile.platform != nullptr ? unpack(tile.platform->strokeTo) : consoleTo;
        paintCard(tile, content, from, to);
        if (tile.platform != nullptr) {
            paintFrame(content, *tile.platform, nullptr, tile.alpha);
        }
        return;
    }
    if (tile.kind == TileKind::Launcher) {
        const auto brand = std::ranges::find_if(brands, [&tile](const Brand& candidate) {
            return tile.logo && candidate.icon == *tile.logo;
        });
        paintCard(tile, content, brand != brands.end() ? brand->from : consoleFrom,
                  brand != brands.end() ? brand->to : consoleTo);
        return;
    }
    if (tile.kind == TileKind::AllGames) {
        paintCard(tile, content, libraryFrom, libraryTo);
        return;
    }
    if (tile.platform != nullptr) {
        // iiSU g24.e: art clipped at the larger art radius, then the platform frame over it.
        const FrameGeometry frame = frameGeometry(content);
        if (tile.art != nullptr) {
            drawTextureRound(RoundRect{content, frame.artRadius}, *tile.art,
                             coverSource(static_cast<float>(tile.art->width),
                                         static_cast<float>(tile.art->height), content),
                             tint);
        } else {
            paintFallback(content, tile.title, tile.alpha);
        }
        paintFrame(content, *tile.platform, tile.glyph, tile.alpha);
    } else if (tile.art != nullptr) {
        // iiSU nx2.l: a cover crop into the content rect at the content radius.
        drawTextureRound(RoundRect{content, geometry.contentRadius}, *tile.art,
                         coverSource(static_cast<float>(tile.art->width),
                                     static_cast<float>(tile.art->height), content),
                         tint);
    } else {
        paintFallback(content, tile.title, tile.alpha);
    }
    paintStores(tile, content);
}

void TilePainter::paintStores(const TileVisual& tile, const Rect& content) {
    const StoreIconRow row = storeIconRow(content, tile.stores.size());
    for (std::size_t i = 0; i < row.count; ++i) {
        const Rect& badge = row.badges[i];
        const Vector2 centre{badge.centreX(), badge.centreY()};
        DrawCircleV(centre, badge.width * 0.5f, withAlpha(badgeDisc, tile.alpha));
        const int pixels = std::max(static_cast<int>(std::lround(badge.width * badgeLogo)), 1);
        if (const Texture* mark = icons_.mask(tile.stores[i], pixels)) {
            DrawTexture(*mark,
                        static_cast<int>(std::lround(centre.x - static_cast<float>(pixels) * 0.5f)),
                        static_cast<int>(std::lround(centre.y - static_cast<float>(pixels) * 0.5f)),
                        withAlpha(WHITE, tile.alpha));
        }
    }
}

void TilePainter::paintFrame(const Rect& rect, const Platform& platform, const Texture* glyph,
                             float alpha) const {
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
    if (glyph != nullptr) {
        // iiSU g24.e: the white glyph contain-fitted into a square at the tab's centre.
        const Rect fit = containFit(static_cast<float>(glyph->width),
                                    static_cast<float>(glyph->height), frame.glyph);
        DrawTexturePro(*glyph,
                       Rectangle{0.0f, 0.0f, static_cast<float>(glyph->width),
                                 static_cast<float>(glyph->height)},
                       Rectangle{fit.x, fit.y, fit.width, fit.height}, Vector2{0.0f, 0.0f}, 0.0f,
                       withAlpha(WHITE, alpha));
    }
}

void TilePainter::paintMark(const TileVisual& tile, const Rect& box, Color ink) {
    if (tile.kind == TileKind::Launcher) {
        const int pixels = std::max(static_cast<int>(std::lround(box.width)), 1);
        const Texture* mark = tile.logo ? icons_.mask(*tile.logo, pixels) : nullptr;
        if (mark != nullptr) {
            DrawTexture(*mark, static_cast<int>(std::lround(box.x)),
                        static_cast<int>(std::lround(box.y)), withAlpha(ink, tile.alpha));
        }
        return;
    }
    // Four rounded tiles in a square: a shelf of games.
    const float cell = box.width * glyphTile;
    const float gap = box.width - 2.0f * cell;
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            fillRoundRect(RoundRect{Rect{box.x + static_cast<float>(column) * (cell + gap),
                                         box.y + static_cast<float>(row) * (cell + gap), cell,
                                         cell},
                                    cell * 0.25f},
                          [ink, alpha = tile.alpha](Vector2, float) {
                              return withAlpha(ink, alpha);
                          });
        }
    }
}

void TilePainter::paintCard(const TileVisual& tile, const Rect& content, Color from, Color to) {
    const FrameGeometry frame = frameGeometry(content);
    const float alpha = tile.alpha;
    fillRoundRect(RoundRect{content, frame.artRadius},
                  [&content, from, to, alpha](Vector2 point, float) {
                      return withAlpha(mix(from, to, diagonal(content, point)), alpha);
                  });
    const Color ink = luminance(mix(from, to, 0.5f)) > 0.6f ? consoleDarkInk : WHITE;

    // One name size and one count size for every card, so the typeface loads two faces; a
    // name too wide for one line breaks at a space.
    const float side = std::min(content.width, content.height);
    const float room = content.width - 2.0f * std::max(frame.tab, frame.stroke * 2.0f);
    const TextStyle name{type().emForLineBox(side * 0.15f)};
    const TextStyle count{type().emForLineBox(side * 0.1f)};
    const std::vector<std::string_view> lines = breakLines(tile.title, name, room);
    const float nameBox = type().lineBox(name);
    const float gap = side * 0.04f;
    const bool marked = tile.kind == TileKind::Launcher || tile.kind == TileKind::AllGames;
    const float mark = marked ? side * markSide : 0.0f;
    const float markRoom = marked ? side * markGap : 0.0f;
    const float block = mark + markRoom + static_cast<float>(lines.size()) * nameBox + gap +
                        type().lineBox(count);
    float y = content.centreY() - block * 0.5f;
    if (marked) {
        paintMark(tile, Rect{content.centreX() - mark * 0.5f, y, mark, mark}, ink);
    }
    y += mark + markRoom + nameBox * 0.5f;
    for (const std::string_view line : lines) {
        type().drawCentred(line, content.centreX() - type().measure(line, name) * 0.5f, y, name,
                           withAlpha(ink, alpha));
        y += nameBox;
    }
    const float countY = y - nameBox * 0.5f + gap + type().lineBox(count) * 0.5f;
    type().drawCentred(tile.caption, content.centreX() - type().measure(tile.caption, count) * 0.5f,
                       countY, count, withAlpha(ink, alpha * 0.72f));
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
    const TextStyle text{type().emForLineBox(
        std::clamp(std::min(content.width, content.height) * 0.34f, 34.0f, 92.0f))};
    const float width = type().measure(letter, text);
    type().drawCentred(letter, content.centreX() - width * 0.5f, content.centreY(), text,
                       withAlpha(fallbackInk, alpha));
}

} // namespace iideck::ui

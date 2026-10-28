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

#include "progress_spinner.hpp"

namespace opensu::ui {

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
constexpr std::array<Colour, 10> ringStops{
    Colour{0x71, 0xE0, 0xFF, 255}, Colour{0x20, 0xDE, 0xFF, 255}, Colour{0x2C, 0x88, 0xFF, 255},
    Colour{0x77, 0x47, 0xFF, 255}, Colour{0xC5, 0x6E, 0xFF, 255}, Colour{0xC5, 0x6E, 0xFF, 255},
    Colour{0x77, 0x47, 0xFF, 255}, Colour{0x2C, 0x88, 0xFF, 255}, Colour{0x20, 0xDE, 0xFF, 255},
    Colour{0x71, 0xE0, 0xFF, 255},
};
// iiSU nx2.j: the ring under the chrome is inset 1.7 frames, the one above 0.9.
constexpr float ringUnderInset = 1.7f;
constexpr float ringOverInset = 0.9f;

// iiSU nx2.u: no-art letter is #66FFFFFF, clamp(0.34 min, 34, 92) px.
constexpr Colour fallbackInk{0xFF, 0xFF, 0xFF, 0x66};

// A cover on its way: the spinner is the focus ring's cyan, a third of the tile.
constexpr Colour loadingInk{0x71, 0xE0, 0xFF, 255};
constexpr float loadingSize = 0.3f;
constexpr float loadingMinSize = 24.0f;
constexpr float loadingStroke = 0.11f;

// A console with no platform colours of its own: iiSU's prompt ink, lightened.
constexpr Colour consoleFrom{0x8A, 0x82, 0x9C, 255};
constexpr Colour consoleTo{0x4D, 0x46, 0x55, 255};
constexpr Colour consoleDarkInk{0x2B, 0x27, 0x33, 255};
constexpr Colour consoleDepth{0x1B, 0x17, 0x26, 255};
constexpr float consoleDeepen = 0.45f;

/// A launcher's card gradient, from its brand's colours.
struct Brand {
    Icon icon;
    Colour from;
    Colour to;
};
constexpr std::array<Brand, 3> brands{
    Brand{Icon::Steam, Colour{0x2A, 0x47, 0x5E, 255}, Colour{0x1B, 0x28, 0x38, 255}},
    Brand{Icon::Epic, Colour{0x3A, 0x3A, 0x3F, 255}, Colour{0x16, 0x16, 0x1A, 255}},
    Brand{Icon::Gog, Colour{0x86, 0x32, 0x8A, 255}, Colour{0x4A, 0x1A, 0x4D, 255}},
};
// The combined library's card: a calm violet apart from the stores' colours.
constexpr Colour libraryFrom{0x5B, 0x4F, 0x9A, 255};
constexpr Colour libraryTo{0x2F, 0x28, 0x5E, 255};
// Giveaways' card: a warm red apart from the stores and the library.
constexpr Colour giveawaysFrom{0xB8, 0x3A, 0x52, 255};
constexpr Colour giveawaysTo{0x5E, 0x1C, 0x33, 255};
// Add a game's card: a teal apart from the stores, the library and Giveaways.
constexpr Colour addGameFrom{0x2E, 0x8C, 0x86, 255};
constexpr Colour addGameTo{0x15, 0x45, 0x4A, 255};
// A store badge's disc, dark so a white logo reads over any cover.
constexpr Colour badgeDisc{0x16, 0x13, 0x1E, 200};
constexpr float badgeLogo = 0.62f;
/// The update arrow against the badge's radius: half its length, its shaft, its head.
constexpr float updateArrowReach = 0.55f;
constexpr float updateArrowWidth = 0.22f;
constexpr float updateArrowHead = 0.42f;
// A folder card's mark is a quarter of the card; the library glyph is four tiles of it.
constexpr float markSide = 0.26f;
// A console's drawing is the card's picture, so it is drawn larger than a store's logo.
constexpr float consoleMarkSide = 0.36f;
constexpr float markGap = 0.05f;
constexpr float glyphTile = 0.44f;

struct Stop {
    float at;
    Colour colour;
};

/// `text` as one line, or two broken at the last space that lets the first fit `room`.
std::vector<std::string_view> breakLines(gpu2d::ui::Typesetter& typesetter, std::string_view text,
                                         float size, float room) {
    if (typesetter.measure(text, size) <= room) {
        return {text};
    }
    for (std::size_t space = text.rfind(' '); space != std::string_view::npos && space > 0;
         space = text.rfind(' ', space - 1)) {
        const std::string_view first = text.substr(0, space);
        if (typesetter.measure(first, size) <= room || first.find(' ') == std::string_view::npos) {
            return {first, text.substr(space + 1)};
        }
    }
    return {text};
}

Colour alphaColour(int r, int g, int b, float alpha) noexcept {
    return Colour{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                  static_cast<std::uint8_t>(b),
                  static_cast<std::uint8_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f))};
}

Colour sample(std::span<const Stop> stops, float t) noexcept {
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
float diagonal(const Rect& rect, Point point) noexcept {
    return ((point.x - rect.x) + (point.y - rect.y)) / std::max(rect.width + rect.height, 1.0f);
}

} // namespace

gpu2d::Rect coverSource(float width, float height, const Rect& target) noexcept {
    const float sourceWidth = std::max(width, 1.0f);
    const float sourceHeight = std::max(height, 1.0f);
    const float targetAspect = std::max(target.width, 1.0f) / std::max(target.height, 1.0f);
    const float sourceAspect = sourceWidth / sourceHeight;
    // iiSU e01.k: keep the full side the target's aspect allows, centred.
    const float cropWidth = sourceAspect > targetAspect ? sourceHeight * targetAspect : sourceWidth;
    const float cropHeight =
        sourceAspect > targetAspect ? sourceHeight : sourceWidth / targetAspect;
    return gpu2d::Rect{(sourceWidth - cropWidth) * 0.5f, (sourceHeight - cropHeight) * 0.5f,
                       cropWidth, cropHeight};
}

void TilePainter::paint(Canvas& canvas, const TileVisual& tile) {
    if (tile.alpha <= 0.0f) {
        return;
    }
    const TileGeometry geometry = tileGeometry(tile.rect, tile.cell);
    // iiSU nx2.j: the canvas is scaled about the tile centre.
    const PanelScope scaled{canvas, Point{tile.rect.centreX(), tile.rect.centreY()}, tile.scale};
    const ChromeVariant& variant = tile.dark ? darkChrome : lightChrome;
    const bool selected = tile.focused && tile.selectionRing;
    paintShadow(canvas, geometry, variant, tile.alpha);
    if (selected) {
        paintRing(canvas, geometry, {ringUnderInset, tile.ringDegrees, tile.alpha});
    }
    paintChrome(canvas, geometry, variant, selected, tile.alpha);
    if (!tile.placeholder) {
        paintContent(canvas, tile, geometry);
    }
    if (selected) {
        paintRing(canvas, geometry, {ringOverInset, tile.ringDegrees, tile.alpha});
    }
}

void TilePainter::paintShadow(Canvas& canvas, const TileGeometry& geometry,
                              const ChromeVariant& variant, float alpha) const {
    // iiSU tx2.b.
    const float stroke = geometry.outerStroke;
    const float blur = std::max(2.5f, stroke * variant.shadowBlur);
    const float spread = std::max(0.75f, 1.25f * stroke);
    const float offsetY = std::max(1.25f, 2.1f * stroke);
    const Colour shadow = alphaColour(0, 0, 0, variant.shadowAlpha * alpha);
    RoundRect shape{geometry.outer, geometry.outerRadius};
    shape = shape.grown(spread);
    shape.rect.y += offsetY;
    fillSoft(canvas, shape, blur, [shadow](Point, float) {
        return shadow;
    });
}

void TilePainter::paintRing(Canvas& canvas, const TileGeometry& geometry, const Ring& ring) const {
    const auto [inset, degrees, alpha] = ring;
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
        canvas, outer, inner,
        [cx, cy, turn, alpha](Point point, float) {
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

void TilePainter::paintChrome(Canvas& canvas, const TileGeometry& geometry,
                              const ChromeVariant& variant, bool focused, float alpha) const {
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
    const auto sheenAt = [&sheen, &outer, alpha](Point point, float) {
        return withAlpha(sample(sheen, diagonal(outer, point)), alpha);
    };
    strokeSoft(canvas, strokeRect, std::clamp(1.02f * frame, 3.8f, 10.0f),
               std::clamp(0.72f * frame, 2.8f, 7.2f), clip, sheenAt);
    strokeSoft(canvas, strokeRect, std::clamp(1.18f * stroke, 1.2f, 2.9f), 0.0f, clip, sheenAt);

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
    const Point bottomRight{outer.right() - outer.width * 0.04f,
                            outer.bottom() - outer.height * 0.04f};
    const Point topLeft{outer.x + outer.width * 0.04f, outer.y + outer.height * 0.04f};
    const float glint = std::clamp(0.85f * stroke, 1.0f, 2.2f);
    for (const auto& [centre, stops] : {std::pair{bottomRight, std::span<const Stop>{warm}},
                                        std::pair{topLeft, std::span<const Stop>{cool}}}) {
        strokeSoft(
            canvas, strokeRect, glint, 0.0f, clip,
            [centre, stops, reach, alpha](Point point, float) {
                const float distance = std::hypot(point.x - centre.x, point.y - centre.y);
                return withAlpha(sample(stops, distance / reach), alpha);
            },
            Tessellation{8, 8});
    }

    // 3. A soft white glow just outside the content rect.
    const float glowAlpha = std::min(variant.glowAlpha * chromeK, variant.glowCap);
    const RoundRect glow =
        RoundRect{geometry.content, geometry.contentRadius}.grown(std::max(0.38f * frame, 1.6f));
    const Colour glowColour = alphaColour(255, 255, 255, glowAlpha * alpha);
    strokeSoft(canvas, glow, std::clamp(frame * variant.glowStroke, 3.4f, 8.2f),
               std::clamp(frame * variant.glowBlur, 2.6f, 6.5f), clip, [glowColour](Point, float) {
                   return glowColour;
               });

    // 4. A crisp white edge; 5. a second, stronger one when focused.
    const Colour edge = alphaColour(255, 255, 255, variant.edge * alpha);
    strokeSoft(canvas, strokeRect, std::clamp(stroke, 1.0f, 2.25f), 0.0f, clip,
               [edge](Point, float) {
                   return edge;
               });
    if (focused) {
        const Colour bright = alphaColour(255, 255, 255, variant.focusedEdge * alpha);
        strokeSoft(canvas, strokeRect, std::clamp(1.18f * stroke, 1.3f, 2.4f), 0.0f, clip,
                   [bright](Point, float) {
                       return bright;
                   });
    }
}

void TilePainter::paintContent(Canvas& canvas, const TileVisual& tile,
                               const TileGeometry& geometry) {
    const Rect& content = geometry.content;
    const Colour tint = withAlpha(white, tile.alpha);
    if (tile.kind == TileKind::Console) {
        // The card is the platform's colours deepened, so its frame stands out and white reads.
        const Colour from = tile.platform != nullptr ? mix(unpack(tile.platform->strokeFrom),
                                                           consoleDepth, consoleDeepen)
                                                     : consoleFrom;
        const Colour to = tile.platform != nullptr
                              ? mix(unpack(tile.platform->strokeTo), consoleDepth, consoleDeepen)
                              : consoleTo;
        paintCard(canvas, tile, content, from, to);
        if (tile.platform != nullptr) {
            paintFrame(canvas, content, *tile.platform, tile.console, tile.alpha);
        }
        return;
    }
    if (tile.kind == TileKind::Launcher) {
        const auto brand = std::ranges::find_if(brands, [&tile](const Brand& candidate) {
            return tile.logo && candidate.icon == *tile.logo;
        });
        paintCard(canvas, tile, content, brand != brands.end() ? brand->from : consoleFrom,
                  brand != brands.end() ? brand->to : consoleTo);
        return;
    }
    if (tile.kind == TileKind::AllGames) {
        paintCard(canvas, tile, content, libraryFrom, libraryTo);
        return;
    }
    if (tile.kind == TileKind::Giveaways) {
        paintCard(canvas, tile, content, giveawaysFrom, giveawaysTo);
        return;
    }
    if (tile.kind == TileKind::AddGame) {
        paintCard(canvas, tile, content, addGameFrom, addGameTo);
        return;
    }
    if (tile.platform != nullptr) {
        // iiSU g24.e: art clipped at the larger art radius, then the platform frame over it.
        const FrameGeometry frame = frameGeometry(content);
        if (tile.art != nullptr) {
            drawTextureRound(canvas, RoundRect{content, frame.artRadius}, tile.art->view(),
                             coverSource(static_cast<float>(tile.art->width()),
                                         static_cast<float>(tile.art->height()), content),
                             tint);
        } else {
            paintFallback(canvas, tile, content);
        }
        paintFrame(canvas, content, *tile.platform, tile.console, tile.alpha);
    } else if (tile.art != nullptr) {
        // iiSU nx2.l: a cover crop into the content rect at the content radius.
        drawTextureRound(canvas, RoundRect{content, geometry.contentRadius}, tile.art->view(),
                         coverSource(static_cast<float>(tile.art->width()),
                                     static_cast<float>(tile.art->height()), content),
                         tint);
    } else {
        paintFallback(canvas, tile, content);
    }
    paintStores(canvas, tile, content);
    if (tile.update) {
        paintUpdate(canvas, tile, content);
    }
}

void TilePainter::paintUpdate(Canvas& canvas, const TileVisual& tile, const Rect& content) const {
    const Rect badge = updateBadge(content);
    const Point centre{badge.centreX(), badge.centreY()};
    const float r = badge.width * 0.5f;
    canvas.fillCircle(centre, r, withAlpha(badgeDisc, tile.alpha));
    // A down arrow: the shaft, then the head below it.
    const Colour ink = withAlpha(white, tile.alpha);
    const float head = r * updateArrowHead;
    canvas.line(Point{centre.x, centre.y - r * updateArrowReach},
                Point{centre.x, centre.y + r * updateArrowReach - head}, r * updateArrowWidth, ink);
    canvas.triangle(Point{centre.x - head, centre.y + r * updateArrowReach - head},
                    Point{centre.x + head, centre.y + r * updateArrowReach - head},
                    Point{centre.x, centre.y + r * updateArrowReach}, ink);
}

void TilePainter::paintStores(Canvas& canvas, const TileVisual& tile, const Rect& content) {
    const StoreIconRow row = storeIconRow(content, tile.stores.size());
    for (std::size_t i = 0; i < row.count; ++i) {
        const Rect& badge = row.badges[i];
        const Point centre{badge.centreX(), badge.centreY()};
        canvas.fillCircle(centre, badge.width * 0.5f, withAlpha(badgeDisc, tile.alpha));
        const int pixels = std::max(static_cast<int>(std::lround(badge.width * badgeLogo)), 1);
        if (const gpu2d::Texture* mark = icons_.mask(tile.stores[i], pixels)) {
            const auto side = static_cast<float>(pixels);
            canvas.image(mark->view(), wholeOf(*mark),
                         gpu2d::Rect{std::round(centre.x - side * 0.5f),
                                     std::round(centre.y - side * 0.5f), side, side},
                         withAlpha(white, tile.alpha));
        }
    }
}

void TilePainter::paintFrame(Canvas& canvas, const Rect& rect, const Platform& platform,
                             std::string_view console, float alpha) {
    // The border sprite drawn as its own shapes: a stroke and a top-left tab, both on the
    // sprite's diagonal gradient.
    const FrameGeometry frame = frameGeometry(rect);
    const Colour from = unpack(platform.strokeFrom);
    const Colour to = unpack(platform.strokeTo);
    const auto colour = [&rect, from, to, alpha](Point point, float) {
        return withAlpha(mix(from, to, diagonal(rect, point)), alpha);
    };
    const RoundRect outer{rect, frame.outerRadius};
    fillBand(canvas, outer, outer.grown(-frame.stroke), colour);
    fillRoundRect(canvas, RoundRect{Rect{rect.x, rect.y, frame.tab, frame.tab}, frame.tabRadius},
                  colour);
    // Only the tab's inner corner is round: square its other two inner corners.
    const float corner = std::min(frame.tabRadius, frame.tab * 0.5f);
    const auto square = [&canvas, &colour](const Rect& part) {
        fillRoundRect(canvas, RoundRect{part, 0.0f}, colour);
    };
    square(Rect{rect.x + frame.tab - corner, rect.y, corner, corner});
    square(Rect{rect.x, rect.y + frame.tab - corner, corner, corner});
    if (console.empty()) {
        return;
    }
    // iiSU g24.e: the white glyph in a square at the tab's centre.
    const int pixels = std::max(static_cast<int>(std::lround(frame.glyph.width)), 1);
    if (const gpu2d::Texture* glyph = icons_.mask(consoleGlyphFile(console), pixels)) {
        const auto side = static_cast<float>(pixels);
        canvas.image(glyph->view(), wholeOf(*glyph),
                     gpu2d::Rect{std::round(frame.glyph.centreX() - side * 0.5f),
                                 std::round(frame.glyph.centreY() - side * 0.5f), side, side},
                     withAlpha(white, alpha));
    }
}

void TilePainter::paintMark(Canvas& canvas, const TileVisual& tile, const Rect& box, Colour ink) {
    // A launcher without a logo (the player's own games) takes the library's glyph.
    const bool drawn = tile.kind == TileKind::Console && !tile.console.empty();
    if ((tile.kind == TileKind::Launcher && tile.logo) || drawn) {
        const int pixels = std::max(static_cast<int>(std::lround(box.width)), 1);
        const gpu2d::Texture* mark = drawn ? icons_.mask(consoleCardFile(tile.console), pixels)
                                           : icons_.mask(*tile.logo, pixels);
        if (mark != nullptr) {
            const auto side = static_cast<float>(pixels);
            canvas.image(mark->view(), wholeOf(*mark),
                         gpu2d::Rect{std::round(box.x), std::round(box.y), side, side},
                         withAlpha(ink, tile.alpha));
        }
        return;
    }
    const auto fill = [&canvas, ink, alpha = tile.alpha](const Rect& rect, float radius) {
        fillRoundRect(canvas, RoundRect{rect, radius}, [ink, alpha](Point, float) {
            return withAlpha(ink, alpha);
        });
    };
    if (tile.kind == TileKind::Giveaways) {
        // A wrapped present: lid and box split by the ribbon, two loops of the bow on top.
        const float ribbon = box.width * 0.12f;
        const float half = (box.width - ribbon) * 0.5f;
        const float lidTop = box.y + box.height * 0.22f;
        const float lid = box.height * 0.2f;
        const float inset = box.width * 0.06f;
        const float bodyTop = lidTop + lid + box.height * 0.04f;
        const float body = box.bottom() - bodyTop;
        const float corner = box.width * 0.05f;
        fill(Rect{box.x, lidTop, half, lid}, corner);
        fill(Rect{box.x + half + ribbon, lidTop, half, lid}, corner);
        fill(Rect{box.x + inset, bodyTop, half - inset, body}, corner);
        fill(Rect{box.x + half + ribbon, bodyTop, half - inset, body}, corner);
        const float loop = box.width * 0.2f;
        fill(Rect{box.centreX() - ribbon * 0.5f - loop, box.y, loop, loop}, loop * 0.5f);
        fill(Rect{box.centreX() + ribbon * 0.5f, box.y, loop, loop}, loop * 0.5f);
        return;
    }
    if (tile.kind == TileKind::AddGame) {
        // A plus.
        const float bar = box.width * 0.2f;
        fill(Rect{box.centreX() - bar * 0.5f, box.y, bar, box.height}, bar * 0.3f);
        fill(Rect{box.x, box.centreY() - bar * 0.5f, box.width, bar}, bar * 0.3f);
        return;
    }
    // Four rounded tiles in a square: a shelf of games.
    const float cell = box.width * glyphTile;
    const float gap = box.width - 2.0f * cell;
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            fill(Rect{box.x + static_cast<float>(column) * (cell + gap),
                      box.y + static_cast<float>(row) * (cell + gap), cell, cell},
                 cell * 0.25f);
        }
    }
}

void TilePainter::paintCard(Canvas& canvas, const TileVisual& tile, const Rect& content,
                            Colour from, Colour to) {
    const FrameGeometry frame = frameGeometry(content);
    const float alpha = tile.alpha;
    fillRoundRect(canvas, RoundRect{content, frame.artRadius},
                  [&content, from, to, alpha](Point point, float) {
                      return withAlpha(mix(from, to, diagonal(content, point)), alpha);
                  });
    const Colour ink = luminance(mix(from, to, 0.5f)) > 0.6f ? consoleDarkInk : white;

    // One name size and one count size for every card, so the face rasterises two sizes; a
    // name too wide for one line breaks at a space.
    const float side = std::min(content.width, content.height);
    const float room = content.width - 2.0f * std::max(frame.tab, frame.stroke * 2.0f);
    const float name = text_.sizeForLineHeight(side * 0.15f);
    const float count = text_.sizeForLineHeight(side * 0.1f);
    const std::vector<std::string_view> lines = breakLines(text_, tile.title, name, room);
    const float nameBox = text_.lineHeight(name);
    const float gap = side * 0.04f;
    const bool marked = tile.kind == TileKind::Launcher || tile.kind == TileKind::AllGames ||
                        tile.kind == TileKind::Giveaways || tile.kind == TileKind::AddGame ||
                        (tile.kind == TileKind::Console && !tile.console.empty());
    const float markShare = tile.kind == TileKind::Console ? consoleMarkSide : markSide;
    const float mark = marked ? side * markShare : 0.0f;
    const float markRoom = marked ? side * markGap : 0.0f;
    const float block = mark + markRoom + static_cast<float>(lines.size()) * nameBox + gap +
                        text_.lineHeight(count);
    float y = content.centreY() - block * 0.5f;
    if (marked) {
        paintMark(canvas, tile, Rect{content.centreX() - mark * 0.5f, y, mark, mark}, ink);
    }
    y += mark + markRoom + nameBox * 0.5f;
    for (const std::string_view line : lines) {
        text_.drawCentred(canvas, line,
                          Point{content.centreX() - text_.measure(line, name) * 0.5f, y}, name,
                          withAlpha(ink, alpha));
        y += nameBox;
    }
    const float countY = y - nameBox * 0.5f + gap + text_.lineHeight(count) * 0.5f;
    text_.drawCentred(canvas, tile.caption,
                      Point{content.centreX() - text_.measure(tile.caption, count) * 0.5f, countY},
                      count, withAlpha(ink, alpha * 0.72f));
}

void TilePainter::paintFallback(Canvas& canvas, const TileVisual& tile, const Rect& content) const {
    if (tile.loading) {
        paintLoading(canvas, tile, content);
        return;
    }
    // iiSU yy0.f: the first letter or digit, upper-cased.
    const auto found = std::ranges::find_if(tile.title, [](char c) {
        return std::isalnum(static_cast<std::uint8_t>(c)) != 0;
    });
    if (found == tile.title.end()) {
        return;
    }
    const std::string letter(1, static_cast<char>(std::toupper(static_cast<std::uint8_t>(*found))));
    const float size = text_.sizeForLineHeight(
        std::clamp(std::min(content.width, content.height) * 0.34f, 34.0f, 92.0f));
    const float width = text_.measure(letter, size);
    text_.drawCentred(canvas, letter, Point{content.centreX() - width * 0.5f, content.centreY()},
                      size, withAlpha(fallbackInk, tile.alpha));
}

void TilePainter::paintLoading(Canvas& canvas, const TileVisual& tile, const Rect& content) const {
    const float side = std::min(content.width, content.height);
    const float size = std::max(side * loadingSize, loadingMinSize);
    drawSpinner(canvas, Point{content.centreX(), content.centreY()}, size, size * loadingStroke,
                withAlpha(loadingInk, tile.alpha), tile.seconds);
}

} // namespace opensu::ui

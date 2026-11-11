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

#include "interface_theme.hpp"
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

/// The most of a tile's short side a game's own icon takes.
constexpr float iconShare = 0.6f;
// iiSU nx2.u: no-art letter is #66FFFFFF, clamp(0.34 min, 34, 92) px.
constexpr Colour fallbackInk{0xFF, 0xFF, 0xFF, 0x66};

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
// A photo card's console fills the card edge to edge; most of its glow margin runs off.
constexpr float photoBleed = 1.06f;
// A photo card's name and count stand this far above the card's bottom edge.
constexpr float photoLabelInset = 0.07f;
// Their outline, as a share of each one's size, so they read on any part of the photo.
constexpr float photoOutline = 0.09f;
// Their outline's colour.
constexpr Colour photoEdge{0, 0, 0, 230};
// The most of a card's height its mark, name and count take together.
constexpr float blockMost = 0.86f;
constexpr float markGap = 0.05f;
constexpr float glyphTile = 0.44f;

struct Stop {
    float at;
    Colour colour;
};

/// `text` broken at spaces into lines that each take as many words as fit `room`; a word wider
/// than `room` stands on a line of its own.
std::vector<std::string_view> breakLines(gpu2d::ui::Typesetter& typesetter, std::string_view text,
                                         float size, float room) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    do {
        std::size_t end = std::min(text.find(' ', start), text.size());
        while (end < text.size()) {
            const std::size_t next = std::min(text.find(' ', end + 1), text.size());
            if (typesetter.measure(text.substr(start, next - start), size) > room) {
                break;
            }
            end = next;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    } while (start < text.size());
    return lines;
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
    if (tiles_.style == theme::TileStyle::Channel) {
        paintChannel(canvas, tile, geometry);
        return;
    }
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

void TilePainter::paintChannel(Canvas& canvas, const TileVisual& tile,
                               const TileGeometry& geometry) {
    const float alpha = tile.alpha;
    const RoundRect body{geometry.outer, geometry.outerRadius};
    const float stroke = std::max(geometry.outerStroke, 1.0f);
    const RoundRect rim{body.rect, body.radius};
    if (tile.placeholder) {
        // An empty slot: a faint well, darker along its top as if pressed in.
        const Colour well = withAlpha(colourOf(tiles_.well), alpha);
        fillRoundRect(canvas, body, [well](Point, float) {
            return well;
        });
        const Rect& r = body.rect;
        const Colour shade = withAlpha(colourOf(tiles_.edge), 0.55f * alpha);
        strokeSoft(canvas, rim.grown(-0.5f * stroke), stroke, stroke, rim,
                   [shade, &r](Point point, float) {
                       const float t =
                           std::clamp((point.y - r.y) / std::max(r.height, 1.0f), 0.0f, 1.0f);
                       return withAlpha(shade, 1.0f - 0.7f * t);
                   });
        return;
    }
    // A soft shadow under the body.
    RoundRect shadow = body.grown(0.5f * stroke);
    shadow.rect.y += 1.6f * stroke;
    const Colour dark = alphaColour(0, 0, 0, 0.12f * alpha);
    fillSoft(canvas, shadow, std::max(3.0f, 2.4f * stroke), [dark](Point, float) {
        return dark;
    });
    // The body, white at the top and a touch greyer at its foot.
    const Colour top = withAlpha(colourOf(tiles_.body), alpha);
    const Colour foot = withAlpha(mix(colourOf(tiles_.body), colourOf(tiles_.edge), 0.25f), alpha);
    const Rect& r = body.rect;
    fillRoundRect(canvas, body, [top, foot, &r](Point point, float) {
        return mix(top, foot, std::clamp((point.y - r.y) / std::max(r.height, 1.0f), 0.0f, 1.0f));
    });
    paintContent(canvas, tile, geometry);
    // Gloss over the upper half: white fading out downwards.
    const Colour gloss = alphaColour(255, 255, 255, 0.18f * alpha);
    fillRoundRect(canvas, RoundRect{Rect{r.x, r.y, r.width, r.height * 0.5f}, body.radius},
                  [gloss, &r](Point point, float) {
                      const float t =
                          std::clamp((point.y - r.y) / std::max(r.height * 0.5f, 1.0f), 0.0f, 1.0f);
                      return withAlpha(gloss, 1.0f - t);
                  });
    const Colour edge = withAlpha(colourOf(tiles_.edge), alpha);
    strokeSoft(canvas, rim.grown(-0.5f * stroke), stroke, 0.0f, rim, [edge](Point, float) {
        return edge;
    });
    if (!tile.focused) {
        return;
    }
    // Focus: an accent outline just outside the body, with a soft glow around it.
    const float width = 1.8f * stroke;
    const RoundRect ring = body.grown(1.2f * stroke + 0.5f * width);
    const RoundRect reach = ring.grown(4.0f * width);
    const Colour focus = withAlpha(colourOf(tiles_.focus), alpha);
    strokeSoft(canvas, ring, 3.0f * width, 2.0f * width, reach, [focus](Point, float) {
        return withAlpha(focus, 0.3f);
    });
    strokeSoft(canvas, ring, width, 0.0f, reach, [focus](Point, float) {
        return focus;
    });
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
        const gpu2d::Texture* card = consoleArt_.look(tile.console) == CardLook::Tile
                                         ? consoleArt_.card(tile.console, 0)
                                         : nullptr;
        if (card != nullptr) {
            // A tile card is the whole tile: it carries the glyph and its own frame.
            drawTextureRound(canvas, RoundRect{content, geometry.contentRadius}, card->view(),
                             coverSource(static_cast<float>(card->width()),
                                         static_cast<float>(card->height()), content),
                             tint);
            return;
        }
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
    if (tile.kind == TileKind::Add) {
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
    // iiSU g24.e: the white glyph contain-fitted into a square at the tab's centre.
    const int pixels = std::max(static_cast<int>(std::lround(frame.glyph.width)), 1);
    if (const gpu2d::Texture* glyph = consoleArt_.glyph(console, pixels)) {
        const auto side = static_cast<float>(pixels);
        const auto width = static_cast<float>(glyph->width());
        const auto height = static_cast<float>(glyph->height());
        const float fit = side / std::max({width, height, 1.0f});
        canvas.image(
            glyph->view(), wholeOf(*glyph),
            gpu2d::Rect{
                std::round(frame.glyph.centreX() - side * 0.5f) + (side - width * fit) * 0.5f,
                std::round(frame.glyph.centreY() - side * 0.5f) + (side - height * fit) * 0.5f,
                width * fit, height * fit},
            withAlpha(white, alpha));
    }
}

void TilePainter::paintMark(Canvas& canvas, const TileVisual& tile, const Rect& box, Colour ink) {
    // A launcher without a logo (the player's own games) takes the library's glyph.
    if (tile.kind == TileKind::Console) {
        const int pixels =
            std::max(static_cast<int>(std::lround(std::max(box.width, box.height))), 1);
        if (const gpu2d::Texture* mark = consoleArt_.card(tile.console, pixels)) {
            const auto width = std::max(static_cast<float>(mark->width()), 1.0f);
            const auto height = std::max(static_cast<float>(mark->height()), 1.0f);
            const float fit = std::min(box.width / width, box.height / height);
            canvas.image(mark->view(), wholeOf(*mark),
                         gpu2d::Rect{std::round(box.x + (box.width - width * fit) * 0.5f),
                                     std::round(box.y + (box.height - height * fit) * 0.5f),
                                     width * fit, height * fit},
                         withAlpha(ink, tile.alpha));
        }
        return;
    }
    if (tile.kind == TileKind::Launcher && tile.logo) {
        const int pixels = std::max(static_cast<int>(std::lround(box.width)), 1);
        if (const gpu2d::Texture* mark = icons_.mask(*tile.logo, pixels)) {
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
    if (tile.kind == TileKind::Add) {
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

void TilePainter::paintPhoto(Canvas& canvas, const TileVisual& tile, const RoundRect& card) {
    const Rect& content = card.rect;
    const int pixels =
        std::max(static_cast<int>(std::lround(std::max(content.width, content.height))), 1);
    const gpu2d::Texture* photo = consoleArt_.card(tile.console, pixels);
    if (photo == nullptr) {
        return;
    }
    const auto width = std::max(static_cast<float>(photo->width()), 1.0f);
    const auto height = std::max(static_cast<float>(photo->height()), 1.0f);
    // The console's longer side spans the card; only its glow margin is cut off.
    const float scale = std::min(content.width / width, content.height / height) * photoBleed;
    const Rect placed{content.centreX() - width * scale * 0.5f,
                      content.centreY() - height * scale * 0.5f, width * scale, height * scale};
    const float left = std::max(placed.x, content.x);
    const float top = std::max(placed.y, content.y);
    const float right = std::min(placed.right(), content.right());
    const float bottom = std::min(placed.bottom(), content.bottom());
    if (right <= left || bottom <= top) {
        return;
    }
    const gpu2d::Rect source{(left - placed.x) / scale, (top - placed.y) / scale,
                             (right - left) / scale, (bottom - top) / scale};
    drawTextureRound(canvas, RoundRect{Rect{left, top, right - left, bottom - top}, card.radius},
                     photo->view(), source, withAlpha(white, tile.alpha));
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
    // name or count too wide for one line breaks at spaces.
    const float side = std::min(content.width, content.height);
    const float room = content.width - 2.0f * std::max(frame.tab, frame.stroke * 2.0f);
    const float name = text_.sizeForLineHeight(side * 0.15f);
    const float count = text_.sizeForLineHeight(side * 0.1f);
    const std::vector<std::string_view> lines = breakLines(text_, tile.title, name, room);
    const std::vector<std::string_view> captions = breakLines(text_, tile.caption, count, room);
    const float nameBox = text_.lineHeight(name);
    const float countBox = text_.lineHeight(count);
    const float countBlock = static_cast<float>(captions.size()) * countBox;
    const float gap = side * 0.04f;
    const bool marked =
        tile.kind == TileKind::Launcher || tile.kind == TileKind::AllGames ||
        tile.kind == TileKind::Giveaways || tile.kind == TileKind::Add ||
        (tile.kind == TileKind::Console && consoleArt_.look(tile.console) != CardLook::None);
    if (tile.kind == TileKind::Console && consoleArt_.look(tile.console) == CardLook::Picture) {
        // A photo fills the card; the name and count stand over its foot, outlined.
        paintPhoto(canvas, tile, RoundRect{content, frame.artRadius});
        const float label = static_cast<float>(lines.size()) * nameBox + gap + countBlock;
        float y = content.bottom() - side * photoLabelInset - label + nameBox * 0.5f;
        for (const std::string_view line : lines) {
            text_.drawCentredOutlined(
                canvas, line, Point{content.centreX() - text_.measure(line, name) * 0.5f, y}, name,
                withAlpha(white, alpha), name * photoOutline, withAlpha(photoEdge, alpha));
            y += nameBox;
        }
        float countY = y - nameBox * 0.5f + gap + countBox * 0.5f;
        for (const std::string_view caption : captions) {
            text_.drawCentredOutlined(
                canvas, caption,
                Point{content.centreX() - text_.measure(caption, count) * 0.5f, countY}, count,
                withAlpha(white, alpha), count * photoOutline, withAlpha(photoEdge, alpha));
            countY += countBox;
        }
        return;
    }
    const float markRoom = marked ? side * markGap : 0.0f;
    const float words = markRoom + static_cast<float>(lines.size()) * nameBox + gap + countBlock;
    // The mark gives way to a two-line name, so the block keeps a margin inside the card.
    const float share = tile.kind == TileKind::Console ? consoleMarkSide : markSide;
    const float mark = marked ? std::max(std::min(side * share, content.height * blockMost - words),
                                         side * markSide)
                              : 0.0f;
    const float block = mark + words;
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
    float countY = y - nameBox * 0.5f + gap + countBox * 0.5f;
    for (const std::string_view caption : captions) {
        text_.drawCentred(canvas, caption,
                          Point{content.centreX() - text_.measure(caption, count) * 0.5f, countY},
                          count, withAlpha(ink, alpha * 0.72f));
        countY += countBox;
    }
}

void TilePainter::paintIcon(Canvas& canvas, const TileVisual& tile, const Rect& content) {
    const gpu2d::Texture& icon = *tile.icon;
    const auto width = static_cast<float>(icon.width());
    const auto height = static_cast<float>(icon.height());
    const float room = std::min(content.width, content.height) * iconShare;
    const float fit = room / std::max(width, height);
    // A small icon grows by whole pixels, so nearest sampling keeps it crisp.
    const float scale = fit >= 1.0f ? std::floor(fit) : fit;
    const float drawnWidth = width * scale;
    const float drawnHeight = height * scale;
    canvas.image(icon.view(), wholeOf(icon),
                 gpu2d::Rect{std::round(content.centreX() - drawnWidth * 0.5f),
                             std::round(content.centreY() - drawnHeight * 0.5f), drawnWidth,
                             drawnHeight},
                 withAlpha(white, tile.alpha));
}

void TilePainter::paintFallback(Canvas& canvas, const TileVisual& tile, const Rect& content) const {
    if (tile.icon != nullptr) {
        paintIcon(canvas, tile, content);
        return;
    }
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
    drawCoverLoading(canvas, content, tile.alpha, tile.seconds);
}

} // namespace opensu::ui

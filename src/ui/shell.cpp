// The drawn home screen.
//
// Geometry, focus movement and drawing live here. The shell never reads the
// library directly: the app pushes a catalog in and the shell renders it.
#include "shell.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <ranges>

#include "lucent/log.h"

#include "platform.hpp"
#include "typeface.hpp"

namespace iideck::ui {
namespace {

/// A scissor region that restores clipping when it goes out of scope.
///
/// raylib's BeginScissorMode/EndScissorMode are not a stack: EndScissorMode turns
/// clipping off entirely, so a region left open silently blanks every draw call
/// after it. Tiles are drawn in a loop, so one missed End is a black screen.
class ScopedScissor {
  public:
    explicit ScopedScissor(const Rectangle& area) {
        const int left = std::max(static_cast<int>(area.x), 0);
        const int top = std::max(static_cast<int>(area.y), 0);
        const int right = std::min(static_cast<int>(area.x + area.width), GetScreenWidth());
        const int bottom = std::min(static_cast<int>(area.y + area.height), GetScreenHeight());
        if (right <= left || bottom <= top) {
            // An empty clip is still a clip: leaving the caller's region in place
            // would draw outside the region it asked to confine to.
            BeginScissorMode(0, 0, 0, 0);
            return;
        }
        BeginScissorMode(left, top, right - left, bottom - top);
    }

    ~ScopedScissor() {
        EndScissorMode();
    }

    ScopedScissor(const ScopedScissor&) = delete;
    ScopedScissor& operator=(const ScopedScissor&) = delete;
};

namespace fs = std::filesystem;

/// Frames a toast stays up for.
constexpr int toastLifetime = 240;

/// The corner radius for a tile, as a fraction of its short side.
float radiusFor(float side) {
    return side * palette::radiusFraction;
}

/// Draws a rounded rectangle filled with a single colour.
void fillRounded(Rectangle rect, float roundness, Color colour) {
    DrawRectangleRounded(rect, roundness, 12, colour);
}

/// Draws a rectangle filled with a left-to-right gradient.
void fillGradient(Rectangle rect, Color from, Color to) {
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rect.width), static_cast<int>(rect.height), from, to);
}

/// Draws a rounded rectangle outline.
void strokeRounded(Rectangle rect, float roundness, float thickness, Color colour) {
    DrawRectangleRoundedLinesEx(rect, roundness, 12, thickness, colour);
}

/// A colour derived from a title, so a game with no artwork always gets the same
/// generated card. raylib converts colours to HSV but not back, so the hue to
/// RGB conversion is done here.
Colour cardColour(std::string_view title, float value) {
    std::uint32_t hash = 2166136261u;
    for (const char c : title) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    const float hue = static_cast<float>(hash % 360u);
    constexpr float saturation = 0.62f;

    const float chroma = value * saturation;
    const float second = chroma * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    const float match = value - chroma;
    const float sector = std::fmod(hue / 60.0f, 6.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (sector < 1.0f) {
        r = chroma;
        g = second;
    } else if (sector < 2.0f) {
        r = second;
        g = chroma;
    } else if (sector < 3.0f) {
        g = chroma;
        b = second;
    } else if (sector < 4.0f) {
        g = second;
        b = chroma;
    } else if (sector < 5.0f) {
        r = second;
        b = chroma;
    } else {
        r = chroma;
        b = second;
    }
    return Colour{static_cast<std::uint8_t>((r + match) * 255.0f),
                  static_cast<std::uint8_t>((g + match) * 255.0f),
                  static_cast<std::uint8_t>((b + match) * 255.0f), 255};
}

std::string lighten(std::string_view text, std::string_view suffix) {
    std::string out{text};
    if (!suffix.empty()) {
        out.append(suffix);
    }
    return out;
}

} // namespace

Shell::Shell(int width, int height) : width_{width}, height_{height} {
    relayout();
}

Shell::~Shell() {
    unloadArtwork();
}

float Shell::unit() const {
    return static_cast<float>(std::min(width_, height_)) / 100.0f;
}

Rectangle Shell::gridRect() const {
    const float u = unit();
    const float padding = u;
    const float top = u * 9.0f;
    const float bottom = u * 8.0f;
    return Rectangle{padding, top, static_cast<float>(width_) - 2.0f * padding,
                     static_cast<float>(height_) - top - bottom};
}

int Shell::columns() const {
    if (width_ < 900) {
        return 4;
    }
    if (width_ < 1400) {
        return 6;
    }
    if (width_ < 1800) {
        return 7;
    }
    return 9;
}

int Shell::rows() const {
    const Rectangle area = gridRect();
    const int cols = std::max(columns(), 1);
    const int cell = static_cast<int>(area.width) / cols;
    if (cell <= 0) {
        return 1;
    }
    return std::max(static_cast<int>(area.height) / cell, 1);
}

int Shell::capacity() const {
    return columns() * rows();
}

Rectangle Shell::topPillRect() const {
    const float u = unit();
    const float height = u * 6.0f;
    const float width = static_cast<float>(width_) * 0.42f;
    return Rectangle{(static_cast<float>(width_) - width) / 2.0f, u * 1.5f, width, height};
}

void Shell::setSize(int width, int height) {
    width_ = width;
    height_ = height;
    relayout();
}

void Shell::setPlatforms(Platforms platforms) {
    platforms_ = std::move(platforms);
}

const Platform* Shell::platformFor(const library::Game& game) const {
    // A ROM belongs to a system, and the pack's console names are those systems.
    // Everything else belongs to a store, and the pack has a border per store.
    if (game.source == library::Source::Rom && !game.sourceId.empty()) {
        if (const auto found = platforms_.find(game.sourceId); found) {
            return &*found;
        }
        return nullptr;
    }
    return platforms_.forSource(game.source);
}

void Shell::setCatalog(std::vector<library::Game> games) {
    releaseTextures();

    // The reference leads with one hero tile, then wide ones, then squares.
    // Tile sizes otherwise come from play recency, filled by the catalog.
    tiles_.clear();
    tiles_.reserve(games.size());
    for (std::size_t index = 0; index < games.size(); ++index) {
        Tile tile;
        tile.game = std::move(games[index]);
        // The reference opens with one hero tile, then two wide ones, then
        // squares. The shape is fixed rather than left to play recency, so the
        // grid always has the mix that reads as a home screen.
        if (index == 0) {
            tile.columns = 2;
            tile.rows = 2;
        } else if (index <= 2) {
            tile.columns = 2;
            tile.rows = 1;
        }
        tiles_.push_back(std::move(tile));
    }
    focus_ = 0;
    page_ = 0;
    relayout();
}

namespace {

/// Shortens a label with an ellipsis until it fits a width, so a featured tile's
/// caption does not run under its hint chip.
std::string clip(std::string_view text, float maxWidth, int fontSize) {
    const std::string full{text};
    if (type().measure(full.c_str(), fontSize) <= maxWidth) {
        return full;
    }
    std::string out{full};
    while (out.size() > 1 && type().measure((out + "...").c_str(), fontSize) > maxWidth) {
        out.pop_back();
    }
    out.append("...");
    return out;
}

/// Loads a texture, returning an empty one when the file is missing or is not an
/// image raylib can read.
Texture loadArt(const fs::path& path) {
    if (path.empty() || !fs::is_regular_file(path)) {
        return Texture{};
    }
    return LoadTexture(path.string().c_str());
}

} // namespace

void Shell::loadArtwork() {
    for (Tile& tile : tiles_) {
        tile.portrait = loadArt(tile.game.artwork);
        tile.wide = loadArt(tile.game.artworkWide);
        tile.hasPortrait = tile.portrait.id != 0;
        tile.hasWide = tile.wide.id != 0;
    }
}

std::size_t Shell::loadedArtwork() const noexcept {
    std::size_t count = 0;
    for (const Tile& tile : tiles_) {
        count += tile.hasPortrait ? 1 : 0;
    }
    return count;
}

void Shell::unloadArtwork() {
    releaseTextures();
}

void Shell::releaseTextures() {
    for (Tile& tile : tiles_) {
        if (tile.hasPortrait) {
            UnloadTexture(tile.portrait);
            tile.hasPortrait = false;
        }
        if (tile.hasWide) {
            UnloadTexture(tile.wide);
            tile.hasWide = false;
        }
    }
}

void Shell::place(std::size_t index, const Rectangle& area, int x, int y, int cellW, int gap) {
    Tile& tile = tiles_[index];
    tile.rect = Rectangle{
        area.x + static_cast<float>(x),
        area.y + static_cast<float>(y),
        static_cast<float>(tile.columns * cellW + (tile.columns - 1) * gap),
        static_cast<float>(tile.rows * rowHeight_ + (tile.rows - 1) * gap),
    };
}

void Shell::relayout() {
    // Pages are rebuilt from scratch: leaving the previous ones would leave the
    // empty placeholder page first, and nothing would be visible.
    pages_.clear();
    if (tiles_.empty()) {
        pages_.push_back({});
        rowHeight_ = 0;
        return;
    }

    for (Tile& tile : tiles_) {
        tile.platform = platformFor(tile.game);
    }

    const int cols = columns();
    const Rectangle area = gridRect();
    const int gap = std::max(static_cast<int>(std::min(area.width, area.height) * 0.014f), 4);
    const int cellW = (static_cast<int>(area.width) - gap * (cols - 1)) / std::max(cols, 1);
    rowHeight_ = cellW;

    for (Tile& tile : tiles_) {
        // A tile can never be wider than the grid.
        tile.columns = std::min(tile.columns, cols);
    }

    // Bands are grouped first, then placed. A band holds tiles side by side and
    // is as tall as its tallest tile, so a two-row tile makes the whole band two
    // rows. Within a band the tall tile goes last, which is what leaves the
    // squares filling the space beside it rather than a hole.
    std::vector<std::vector<std::size_t>> bands;
    {
        std::vector<std::size_t> band;
        int bandColumns = 0;
        int bandRows = 0;
        for (std::size_t index = 0; index < tiles_.size(); ++index) {
            const Tile& tile = tiles_[index];
            const bool fitsWidth = bandColumns + tile.columns <= cols;
            // A band stays at most two rows tall, which is what keeps a tall tile
            // from turning the whole grid into a stack of wide bands.
            const bool fitsHeight = bandRows == 0 || std::max(bandRows, tile.rows) <= 2;
            if (!band.empty() && (!fitsWidth || !fitsHeight)) {
                bands.push_back(band);
                band.clear();
                bandColumns = 0;
                bandRows = 0;
            }
            band.push_back(index);
            bandColumns += tile.columns;
            bandRows = std::max(bandRows, tile.rows);
        }
        if (!band.empty()) {
            bands.push_back(band);
        }
    }

    // Bands become pages, so a page is a whole number of rows.
    const int capacity = this->capacity();
    std::vector<std::size_t> page;
    int used = 0;
    int y = 0;
    for (const std::vector<std::size_t>& band : bands) {
        const int bandColumns =
            std::accumulate(band.begin(), band.end(), 0, [this](int sum, std::size_t index) {
                return sum + tiles_[index].columns;
            });
        const int bandRows =
            std::accumulate(band.begin(), band.end(), 0, [this](int, std::size_t index) {
                return std::max(tiles_[index].rows, 1);
            });
        const int cost = bandColumns * bandRows;

        if (!page.empty() && used + cost > capacity) {
            pages_.push_back(page);
            page.clear();
            used = 0;
            y = 0;
        }

        // Single-row tiles fill from the left; a taller tile sits at the end.
        int x = 0;
        for (const std::size_t index : band) {
            if (tiles_[index].rows > 1) {
                continue;
            }
            place(index, area, x, y, cellW, gap);
            x += tiles_[index].columns * cellW + (tiles_[index].columns - 1) * gap + gap;
        }
        for (const std::size_t index : band) {
            if (tiles_[index].rows <= 1) {
                continue;
            }
            place(index, area, x, y, cellW, gap);
            x += tiles_[index].columns * cellW + (tiles_[index].columns - 1) * gap + gap;
        }

        for (const std::size_t index : band) {
            page.push_back(index);
        }
        used += cost;
        y += bandRows * rowHeight_ + (bandRows - 1) * gap + gap;
    }
    pages_.push_back(page);

    if (page_ >= static_cast<int>(pages_.size())) {
        page_ = 0;
    }
    refreshFocus();
}

bool Shell::visible(std::size_t index) const {
    if (page_ < 0 || page_ >= static_cast<int>(pages_.size())) {
        return false;
    }
    const std::vector<std::size_t>& page = pages_[static_cast<std::size_t>(page_)];
    return std::ranges::find(page, index) != page.end();
}

void Shell::refreshFocus() {
    if (tiles_.empty()) {
        return;
    }
    if (!visible(focus_)) {
        if (page_ < static_cast<int>(pages_.size()) &&
            !pages_[static_cast<std::size_t>(page_)].empty()) {
            focus_ = pages_[static_cast<std::size_t>(page_)].front();
        } else {
            focus_ = 0;
        }
    }
    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        tiles_[index].focused = index == focus_;
    }
}

void Shell::resetFocus() {
    page_ = 0;
    focus_ = 0;
    refreshFocus();
}

const library::Game* Shell::focusedGame() const {
    if (focus_ >= tiles_.size()) {
        return nullptr;
    }
    return &tiles_[focus_].game;
}

bool Shell::moveFocus(int dx, int dy) {
    if (page_ < 0 || page_ >= static_cast<int>(pages_.size())) {
        return false;
    }
    const std::vector<std::size_t>& page = pages_[static_cast<std::size_t>(page_)];
    const auto from = std::ranges::find(page, focus_);
    if (from == page.end()) {
        return false;
    }

    // Tiles vary in span, so index arithmetic would land on the wrong one. The
    // nearest tile in the pressed direction is chosen by comparing centres.
    const Rectangle origin = tiles_[focus_].rect;
    const float fx = origin.x + origin.width / 2.0f;
    const float fy = origin.y + origin.height / 2.0f;

    std::size_t best = focus_;
    float bestScore = std::numeric_limits<float>::max();
    for (const std::size_t index : page) {
        if (index == focus_) {
            continue;
        }
        const Rectangle rect = tiles_[index].rect;
        const float ox = rect.x + rect.width / 2.0f - fx;
        const float oy = rect.y + rect.height / 2.0f - fy;
        if ((dx > 0 && ox <= 0.0f) || (dx < 0 && ox >= 0.0f)) {
            continue;
        }
        if ((dy > 0 && oy <= 0.0f) || (dy < 0 && oy >= 0.0f)) {
            continue;
        }
        // Prefer the nearest along the pressed axis, then the closest across it.
        const float along = dy != 0 ? std::abs(oy) : std::abs(ox);
        const float across = dy != 0 ? std::abs(ox) : std::abs(oy);
        const float score = along + across * 3.0f;
        if (score < bestScore) {
            bestScore = score;
            best = index;
        }
    }
    if (best == focus_) {
        return false;
    }
    focus_ = best;
    refreshFocus();
    return true;
}

bool Shell::movePage(int delta) {
    const int next = page_ + delta;
    if (next < 0 || next >= static_cast<int>(pages_.size())) {
        return false;
    }
    page_ = next;
    refreshFocus();
    return true;
}

void Shell::setToast(std::string text, bool isError) {
    toast_ = std::move(text);
    toastError_ = isError;
    toastFrames_ = toastLifetime;
}

void Shell::tickToast() {
    if (toastFrames_ > 0) {
        --toastFrames_;
        if (toastFrames_ == 0) {
            toast_.clear();
        }
    }
}

void Shell::draw() {
    BeginDrawing();
    ClearBackground(palette::ground);

    drawDottedGround();
    drawTopBar();
    drawTiles();
    drawFooter();
    drawToast();

    EndDrawing();
}

void Shell::drawDottedGround() {
    const float u = unit();
    const float spacing = std::max(u * 2.2f, 8.0f);
    const float radius = std::max(spacing * 0.09f, 1.0f);
    const Color dot{palette::ink.r, palette::ink.g, palette::ink.b, 24};

    // raylib batches primitives, so a grid of dots is one draw call.
    for (float y = spacing; y < static_cast<float>(height_); y += spacing) {
        for (float x = spacing; x < static_cast<float>(width_); x += spacing) {
            DrawCircleV({x, y}, radius, dot);
        }
    }
}

void Shell::drawTopBar() {
    const float u = unit();
    const Rectangle pill = topPillRect();
    const float roundness = pill.height / 2.0f / std::max(pill.width, 1.0f);

    // A translucent tint, standing in for the reference's glass panel.
    fillRounded(pill, roundness * pill.width / pill.height, Color{0x7c, 0x5c, 0xff, 0x24});
    strokeRounded(pill, roundness * pill.width / pill.height, std::max(u * 0.12f, 1.0f),
                  Color{0x7c, 0x5c, 0xff, 0x40});

    const library::Game* focused = focusedGame();
    if (focused != nullptr) {
        const int size = static_cast<int>(u * 2.2f);
        const float textWidth = type().measure(focused->title.c_str(), size);
        type().draw(focused->title.c_str(), pill.x + (pill.width - textWidth) / 2.0f,
                    pill.y + (pill.height - static_cast<float>(size)) / 2.0f, size, palette::ink);
    }

    const int baseline = static_cast<int>(pill.y + pill.height / 2.0f - u * 0.9f);
    if (!status_.empty()) {
        type().draw(status_.c_str(), static_cast<int>(u), baseline, static_cast<int>(u * 1.6f),
                    palette::inkSoft);
    }
    if (!clock_.empty()) {
        const int size = static_cast<int>(u * 1.6f);
        type().draw(clock_.c_str(),
                    width_ - static_cast<int>(u) - type().measure(clock_.c_str(), size), baseline,
                    size, palette::inkSoft);
    }
}

void drawArtCover(const Texture& art, const Rectangle& tile, Color backdrop) {
    if (art.id == 0 || art.width <= 0 || art.height <= 0) {
        return;
    }

    // Scaled to cover, not to fit: the smallest factor that leaves no gap, so
    // the art keeps its own proportions and the overflow is cropped.
    const float scale = std::max(tile.width / static_cast<float>(art.width),
                                 tile.height / static_cast<float>(art.height));
    const float width = static_cast<float>(art.width) * scale;
    const float height = static_cast<float>(art.height) * scale;
    const Rectangle destination{tile.x + (tile.width - width) / 2.0f,
                                tile.y + (tile.height - height) / 2.0f, width, height};

    // A cover wider than its tile in shape, such as a banner in a square tile,
    // can still leave the tile's edges bare, and bare edges must not be
    // transparent. Anything outside the tile is clipped away.
    const ScopedScissor clip{tile};
    fillRounded(destination, 0.0f, backdrop);
    DrawTexturePro(art,
                   Rectangle{0, 0, static_cast<float>(art.width), static_cast<float>(art.height)},
                   destination, {0, 0}, 0.0f, WHITE);
}

void Shell::drawTiles() {
    const float u = unit();

    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        if (!visible(index)) {
            continue;
        }
        const Tile& tile = tiles_[index];
        const Rectangle r = tile.rect;
        // A featured tile is one spanning columns: the reference gives only these
        // a hint chip, and their captions are shortened so the two never overlap.
        const bool featured = tile.columns > 1;
        const float side = std::min(r.width, r.height);
        const float roundness = std::min(radiusFor(side) / std::max(r.width, 1.0f), 0.5f);

        // A soft shadow, offset down and right, lifting the tile off the ground.
        fillRounded({r.x + u * 0.4f, r.y + u * 0.6f, r.width, r.height}, roundness,
                    Color{palette::ink.r, palette::ink.g, palette::ink.b, 24});
        fillRounded(r, roundness, palette::panel);

        // Artwork, or a generated card when the store has none.
        const Texture art = (tile.columns > 1 && tile.hasWide)
                                ? tile.wide
                                : (tile.hasPortrait ? tile.portrait : Texture{});
        if (art.id != 0) {
            drawArtCover(art, r, palette::panel);
        } else {
            fillGradient(r, cardColour(tile.game.title, 0.62f),
                         cardColour(tile.game.title + "x", 0.44f));
        }

        if (!tile.game.installed) {
            fillRounded(r, roundness, Color{0, 0, 0, 85});
        }

        if (!tile.game.title.empty()) {
            // A scrim so the caption stays legible over any artwork.
            const float scrimHeight = std::max(r.height * 0.34f, u * 3.0f);
            DrawRectangleGradientV(static_cast<int>(r.x),
                                   static_cast<int>(r.y + r.height - scrimHeight),
                                   static_cast<int>(r.width), static_cast<int>(scrimHeight),
                                   Color{0, 0, 0, 0}, Color{0, 0, 0, 192});
            const int size = static_cast<int>(side * 0.072f);
            // A featured tile carries its hint chip on the caption's line, so its
            // caption gets only the space to the chip's left.
            const std::string caption =
                featured ? clip(tile.game.title, r.width * 0.55f, size) : tile.game.title;
            type().draw(caption.c_str(), r.x + side * 0.045f,
                        r.y + r.height - side * 0.10f - static_cast<float>(size) * 0.82f, size,
                        WHITE);
        }

        if (!tile.game.badge.empty()) {
            const int size = static_cast<int>(side * 0.058f);
            const float textWidth = type().measure(tile.game.badge.c_str(), size);
            const Rectangle chip{r.x + side * 0.04f, r.y + side * 0.04f,
                                 textWidth + static_cast<float>(size) * 1.1f,
                                 static_cast<float>(size) * 1.85f};
            fillRounded(chip, 0.5f, Color{0xff, 0xff, 0xff, 232});
            type().draw(tile.game.badge.c_str(), chip.x + static_cast<float>(size) * 0.55f,
                        chip.y + static_cast<float>(size) * 0.45f, size, palette::ink);
        }

        if (featured && !tile.game.hint.empty()) {
            const int size = static_cast<int>(side * 0.058f);
            const float textWidth = type().measure(tile.game.hint.c_str(), size);
            const Rectangle chip{
                r.x + r.width - side * 0.04f - textWidth - static_cast<float>(size) * 1.1f,
                r.y + r.height - static_cast<float>(size) * 2.6f,
                textWidth + static_cast<float>(size) * 1.1f, static_cast<float>(size) * 1.85f};
            fillRounded(chip, 0.5f, Color{0xff, 0xff, 0xff, 224});
            type().draw(tile.game.hint.c_str(), chip.x + static_cast<float>(size) * 0.55f,
                        chip.y + static_cast<float>(size) * 0.45f, size, palette::ink);
        }

        // The platform's frame, or the focus ring when the tile has focus: the
        // reference frames a tile in its console's colour and replaces that with
        // the selection ring when it is selected. Drawn last, over the artwork,
        // since that is the order the reference's own sprite composites in.
        drawPlatformFrame(r, tile.platform, roundness, palette::focusA, palette::focusB,
                          palette::focusC, tile.focused);
    }
}

void Shell::drawFooter() {
    const float u = unit();
    const int baseline = height_ - static_cast<int>(u * 3.0f);

    // Page dots, centred.
    const float dotRadius = std::max(u * 0.42f, 2.0f);
    const float gap = u * 1.6f;
    const int count = std::max(pageCount(), 1);
    const float total = static_cast<float>(count - 1) * gap + 2.0f * dotRadius * 1.25f;
    float x = static_cast<float>(width_) / 2.0f - total / 2.0f + dotRadius * 1.25f;
    for (int page = 0; page < count; ++page) {
        const bool active = page == page_;
        DrawCircleV({x, static_cast<float>(baseline)}, active ? dotRadius * 1.25f : dotRadius,
                    active ? palette::dotActive : palette::dotInactive);
        x += gap;
    }

    // Corner prompts: a rounded key cap followed by the label.
    const auto hint = [this, baseline](const char* key, const char* label, int x) {
        const int size = static_cast<int>(unit() * 1.1f);
        const int keyWidth = type().measure(key, size);
        const int capWidth = keyWidth + static_cast<int>(unit() * 1.2f);
        const int capHeight = size + static_cast<int>(unit() * 0.9f);
        const Rectangle cap{static_cast<float>(x), static_cast<float>(baseline) - capHeight,
                            static_cast<float>(capWidth), static_cast<float>(capHeight)};
        fillRounded(cap, 0.4f, palette::panel);
        type().draw(key, x + static_cast<int>(unit() * 0.6f),
                    baseline - static_cast<int>(unit() * 0.6f), size, palette::ink);
        type().draw(label, x + capWidth + static_cast<int>(unit() * 0.6f),
                    baseline - static_cast<int>(unit() * 0.6f), size, palette::inkSoft);
    };
    hint("A", "Play", static_cast<int>(u * 1.4f));
    const int rightWidth = type().measure("Refresh", static_cast<int>(u * 1.1f));
    hint("X", "Refresh",
         width_ - static_cast<int>(u * 1.4f) - rightWidth - static_cast<int>(u * 2.4f));
}

void Shell::drawToast() {
    if (toast_.empty()) {
        return;
    }
    const float u = unit();
    const int size = static_cast<int>(u * 1.4f);
    const int textWidth = type().measure(toast_.c_str(), size);
    const int padding = static_cast<int>(u * 2.0f);
    const Rectangle box{
        (static_cast<float>(width_) - textWidth - 2 * padding) / 2.0f,
        static_cast<float>(height_) - u * 8.0f,
        static_cast<float>(textWidth) + 2.0f * padding,
        static_cast<float>(size) + static_cast<float>(padding),
    };
    fillRounded(box, 0.5f,
                toastError_ ? Color{0xb3, 0x26, 0x1e, 240} : Color{0x2b, 0x27, 0x33, 240});
    type().draw(toast_.c_str(), static_cast<int>(box.x) + padding / 2,
                static_cast<int>(box.y) + padding / 3, size, WHITE);
}

namespace {

/// The frame's colour at a point, as the reference's sprite runs it: a straight
/// line from one end of the stroke's gradient to the other.
Color strokeAt(const Platform& platform, float t) {
    const auto channel = [](std::uint32_t packed, int shift) {
        return static_cast<float>((packed >> shift) & 0xff);
    };
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return Color{static_cast<std::uint8_t>(
                     channel(platform.strokeFrom, 16) +
                     (channel(platform.strokeTo, 16) - channel(platform.strokeFrom, 16)) * clamped),
                 static_cast<std::uint8_t>(
                     channel(platform.strokeFrom, 8) +
                     (channel(platform.strokeTo, 8) - channel(platform.strokeFrom, 8)) * clamped),
                 static_cast<std::uint8_t>(
                     channel(platform.strokeFrom, 0) +
                     (channel(platform.strokeTo, 0) - channel(platform.strokeFrom, 0)) * clamped),
                 255};
}

/// The gradient runs along the tile's width on the top edge and its height on the
/// sides, which is what the sprite shows: the colour changes as you move down a
/// side, and again as you move across the top.
constexpr int kStrokeSteps = 48;

} // namespace

void drawPlatformFrame(const Rectangle& tile, const Platform* platform, float radius,
                       const Color& focusA, const Color& focusB, const Color& focusC,
                       bool focused) {
    if (platform == nullptr) {
        return;
    }

    // The stroke and tab are fractions of the border sprite's own 1024 canvas, and
    // the tile draws that same fraction of itself. On the reference's tiles the
    // frame reads as an edge; on a grid this dense a 26px frame would be 5px of
    // colour on each side of a 200px tile, so the stroke is scaled to the tile's
    // short side instead, which keeps it proportionally thin on any tile size.
    const float thickness =
        std::clamp(std::min(tile.width, tile.height) * platform->strokeFraction, 1.5f, 4.0f);
    const float tab = std::min(tile.width, tile.height) * platform->tabFraction;

    if (focused) {
        // Focus keeps the shell's own ring: it is how focus is shown, and it
        // outranks the platform's colour.
        const Rectangle ring{tile.x - thickness / 2.0f, tile.y - thickness / 2.0f,
                             tile.width + thickness, tile.height + thickness};
        const float half = ring.width / 2.0f;
        fillGradient({ring.x, ring.y, half, ring.height}, focusA, focusB);
        fillGradient({ring.x + half, ring.y, half, ring.height}, focusB, focusC);
        strokeRounded(tile, radius, std::max(thickness * 0.4f, 1.0f), Color{0xff, 0xff, 0xff, 208});
        return;
    }

    // The frame's corner radius is its own: the sprite's outer edge is rounded and
    // its stroke follows that curve, so the frame cannot be four rectangles.
    const Rectangle clipArea{tile.x - thickness, tile.y - thickness, tile.width + thickness * 2.0f,
                             tile.height + thickness * 2.0f};
    const ScopedScissor clip{clipArea};

    for (int i = 0; i < kStrokeSteps; ++i) {
        const float f0 = static_cast<float>(i) / kStrokeSteps;
        const float f1 = static_cast<float>(i + 1) / kStrokeSteps;
        const Color colour = strokeAt(*platform, f0);
        // Four sides, each a thin gradient run, so the outline reads as one
        // gradient rather than four flat bands.
        const Rectangle top{tile.x + tile.width * f0, tile.y, tile.width * (f1 - f0), thickness};
        DrawRectangleRec(top, colour);
        const Rectangle bottom{tile.x + tile.width * f0, tile.y + tile.height - thickness,
                               tile.width * (f1 - f0), thickness};
        DrawRectangleRec(bottom, colour);
    }
    for (int i = 0; i < kStrokeSteps; ++i) {
        const float f0 = static_cast<float>(i) / kStrokeSteps;
        const float f1 = static_cast<float>(i + 1) / kStrokeSteps;
        const Color colour = strokeAt(*platform, f0);
        const Rectangle left{tile.x, tile.y + tile.height * f0, thickness, tile.height * (f1 - f0)};
        DrawRectangleRec(left, colour);
        const Rectangle right{tile.x + tile.width - thickness, tile.y + tile.height * f0, thickness,
                              tile.height * (f1 - f0)};
        DrawRectangleRec(right, colour);
    }

    // The corners, where four rectangles cannot reach: a rounded stroke in the
    // colour at that corner, so the frame closes.
    for (const Rectangle& corner :
         {Rectangle{tile.x, tile.y, tile.width, tile.height},
          Rectangle{tile.x, tile.y, radius * 2.0f, radius * 2.0f},
          Rectangle{tile.x + tile.width - radius * 2.0f, tile.y, radius * 2.0f, radius * 2.0f},
          Rectangle{tile.x, tile.y + tile.height - radius * 2.0f, radius * 2.0f, radius * 2.0f},
          Rectangle{tile.x + tile.width - radius * 2.0f, tile.y + tile.height - radius * 2.0f,
                    radius * 2.0f, radius * 2.0f}}) {
        strokeRounded(corner, radius, thickness, strokeAt(*platform, 0.5f));
    }

    // The corner tab, in the darker end of the gradient, which is where the
    // sprite's tab sits.
    fillRounded({tile.x - thickness, tile.y - thickness, tab, tab}, radius,
                strokeAt(*platform, 0.0f));
}

} // namespace iideck::ui

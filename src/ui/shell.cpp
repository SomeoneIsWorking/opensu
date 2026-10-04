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

namespace iideck::ui {
namespace {

namespace fs = std::filesystem;

/// Frames a toast stays up for.
constexpr int toastLifetime = 240;

/// The corner radius for a tile, as a fraction of its short side.
float radiusFor(float side)
{
    return side * palette::radiusFraction;
}

/// Draws a rounded rectangle filled with a single colour.
void fillRounded(Rectangle rect, float roundness, Color colour)
{
    DrawRectangleRounded(rect, roundness, 12, colour);
}

/// Draws a rectangle filled with a left-to-right gradient.
void fillGradient(Rectangle rect, Color from, Color to)
{
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y),
        static_cast<int>(rect.width), static_cast<int>(rect.height), from, to);
}

/// Draws a rounded rectangle outline.
void strokeRounded(Rectangle rect, float roundness, float thickness, Color colour)
{
    DrawRectangleRoundedLinesEx(rect, roundness, 12, thickness, colour);
}

/// A colour derived from a title, so a game with no artwork always gets the same
/// generated card. raylib converts colours to HSV but not back, so the hue to
/// RGB conversion is done here.
Colour cardColour(std::string_view title, float value)
{
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

std::string lighten(std::string_view text, std::string_view suffix)
{
    std::string out{text};
    if (!suffix.empty()) {
        out.append(suffix);
    }
    return out;
}

} // namespace

Shell::Shell(int width, int height)
    : width_{width}
    , height_{height}
{
    relayout();
}

Shell::~Shell() { unloadArtwork(); }

float Shell::unit() const
{
    return static_cast<float>(std::min(width_, height_)) / 100.0f;
}

Rectangle Shell::gridRect() const
{
    const float u = unit();
    const float padding = u;
    const float top = u * 9.0f;
    const float bottom = u * 8.0f;
    return Rectangle{padding, top, static_cast<float>(width_) - 2.0f * padding,
        static_cast<float>(height_) - top - bottom};
}

int Shell::columns() const
{
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

int Shell::rows() const
{
    const Rectangle area = gridRect();
    const int cols = std::max(columns(), 1);
    const int cell = static_cast<int>(area.width) / cols;
    if (cell <= 0) {
        return 1;
    }
    return std::max(static_cast<int>(area.height) / cell, 1);
}

int Shell::capacity() const { return columns() * rows(); }

Rectangle Shell::topPillRect() const
{
    const float u = unit();
    const float height = u * 6.0f;
    const float width = static_cast<float>(width_) * 0.42f;
    return Rectangle{(static_cast<float>(width_) - width) / 2.0f, u * 1.5f, width, height};
}

void Shell::setSize(int width, int height)
{
    width_ = width;
    height_ = height;
    relayout();
}

void Shell::setCatalog(std::vector<library::Game> games)
{
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
std::string clip(std::string_view text, float maxWidth, int fontSize)
{
    const std::string full{text};
    if (MeasureText(full.c_str(), fontSize) <= maxWidth) {
        return full;
    }
    std::string out{full};
    while (out.size() > 1 && MeasureText((out + "...").c_str(), fontSize) > maxWidth) {
        out.pop_back();
    }
    out.append("...");
    return out;
}

/// Loads a texture, returning an empty one when the file is missing or is not an
/// image raylib can read.
Texture loadArt(const fs::path& path)
{
    if (path.empty() || !fs::is_regular_file(path)) {
        return Texture{};
    }
    return LoadTexture(path.string().c_str());
}

} // namespace

void Shell::loadArtwork()
{
    for (Tile& tile : tiles_) {
        tile.portrait = loadArt(tile.game.artwork);
        tile.wide = loadArt(tile.game.artworkWide);
        tile.hasPortrait = tile.portrait.id != 0;
        tile.hasWide = tile.wide.id != 0;
    }
}

std::size_t Shell::loadedArtwork() const noexcept
{
    std::size_t count = 0;
    for (const Tile& tile : tiles_) {
        count += tile.hasPortrait ? 1 : 0;
    }
    return count;
}

void Shell::unloadArtwork() { releaseTextures(); }

void Shell::releaseTextures()
{
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

void Shell::place(std::size_t index, const Rectangle& area, int x, int y, int cellW, int gap)
{
    Tile& tile = tiles_[index];
    tile.rect = Rectangle{
        area.x + static_cast<float>(x),
        area.y + static_cast<float>(y),
        static_cast<float>(tile.columns * cellW + (tile.columns - 1) * gap),
        static_cast<float>(tile.rows * rowHeight_ + (tile.rows - 1) * gap),
    };
}

void Shell::relayout()
{
    // Pages are rebuilt from scratch: leaving the previous ones would leave the
    // empty placeholder page first, and nothing would be visible.
    pages_.clear();
    if (tiles_.empty()) {
        pages_.push_back({});
        rowHeight_ = 0;
        return;
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
        const int bandColumns = std::accumulate(band.begin(), band.end(), 0,
            [this](int sum, std::size_t index) { return sum + tiles_[index].columns; });
        const int bandRows = std::accumulate(band.begin(), band.end(), 0,
            [this](int, std::size_t index) { return std::max(tiles_[index].rows, 1); });
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

bool Shell::visible(std::size_t index) const
{
    if (page_ < 0 || page_ >= static_cast<int>(pages_.size())) {
        return false;
    }
    const std::vector<std::size_t>& page = pages_[static_cast<std::size_t>(page_)];
    return std::ranges::find(page, index) != page.end();
}

void Shell::refreshFocus()
{
    if (tiles_.empty()) {
        return;
    }
    if (!visible(focus_)) {
        if (page_ < static_cast<int>(pages_.size()) && !pages_[static_cast<std::size_t>(page_)].empty()) {
            focus_ = pages_[static_cast<std::size_t>(page_)].front();
        } else {
            focus_ = 0;
        }
    }
    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        tiles_[index].focused = index == focus_;
    }
}

void Shell::resetFocus()
{
    page_ = 0;
    focus_ = 0;
    refreshFocus();
}

const library::Game* Shell::focusedGame() const
{
    if (focus_ >= tiles_.size()) {
        return nullptr;
    }
    return &tiles_[focus_].game;
}

bool Shell::moveFocus(int dx, int dy)
{
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

bool Shell::movePage(int delta)
{
    const int next = page_ + delta;
    if (next < 0 || next >= static_cast<int>(pages_.size())) {
        return false;
    }
    page_ = next;
    refreshFocus();
    return true;
}

void Shell::setToast(std::string text, bool isError)
{
    toast_ = std::move(text);
    toastError_ = isError;
    toastFrames_ = toastLifetime;
}

void Shell::tickToast()
{
    if (toastFrames_ > 0) {
        --toastFrames_;
        if (toastFrames_ == 0) {
            toast_.clear();
        }
    }
}

void Shell::draw()
{
    BeginDrawing();
    ClearBackground(palette::ground);

    drawDottedGround();
    drawTopBar();
    drawTiles();
    drawFooter();
    drawToast();

    EndDrawing();
}

void Shell::drawDottedGround()
{
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

void Shell::drawTopBar()
{
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
        DrawText(focused->title.c_str(),
            static_cast<int>(pill.x + (pill.width - MeasureText(focused->title.c_str(), size)) / 2.0f),
            static_cast<int>(pill.y + (pill.height - size) / 2.0f),
            size, palette::ink);
    }

    const int baseline = static_cast<int>(pill.y + pill.height / 2.0f - u * 0.9f);
    if (!status_.empty()) {
        DrawText(status_.c_str(), static_cast<int>(u), baseline, static_cast<int>(u * 1.6f), palette::inkSoft);
    }
    if (!clock_.empty()) {
        const int size = static_cast<int>(u * 1.6f);
        DrawText(clock_.c_str(), width_ - static_cast<int>(u) - MeasureText(clock_.c_str(), size), baseline,
            size, palette::inkSoft);
    }
}

void Shell::drawTiles()
{
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
        const Texture art = (tile.columns > 1 && tile.hasWide) ? tile.wide
                                                               : (tile.hasPortrait ? tile.portrait : Texture{});
        if (art.id != 0) {
            DrawTexturePro(art,
                Rectangle{0, 0, static_cast<float>(art.width), static_cast<float>(art.height)},
                r, {0, 0}, 0.0f, WHITE);
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
            DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y + r.height - scrimHeight),
                static_cast<int>(r.width), static_cast<int>(scrimHeight),
                Color{0, 0, 0, 0}, Color{0, 0, 0, 192});
            const std::string caption = featured ? clip(tile.game.title, r.width - u * 12.0f,
                                                         static_cast<int>(u * 1.3f))
                                                 : tile.game.title;
            DrawText(caption.c_str(), static_cast<int>(r.x + u),
                static_cast<int>(r.y + r.height - u * 1.9f), static_cast<int>(u * 1.3f), WHITE);
        }

        if (!tile.game.badge.empty()) {
            const int size = static_cast<int>(u * 1.1f);
            const int textWidth = MeasureText(tile.game.badge.c_str(), size);
            const Rectangle chip{r.x + u * 0.8f, r.y + u * 0.8f,
                static_cast<float>(textWidth) + u * 1.4f, static_cast<float>(size) + u * 1.0f};
            fillRounded(chip, 0.5f, Color{0xff, 0xff, 0xff, 232});
            DrawText(tile.game.badge.c_str(), static_cast<int>(chip.x + u * 0.7f),
                static_cast<int>(chip.y + u * 0.5f), size, palette::ink);
        }

        if (featured && !tile.game.hint.empty()) {
            const int size = static_cast<int>(u * 1.1f);
            const int textWidth = MeasureText(tile.game.hint.c_str(), size);
            const Rectangle chip{r.x + r.width - u * 0.8f - textWidth - u * 1.4f,
                r.y + r.height - u * 2.6f,
                static_cast<float>(textWidth) + u * 1.4f, static_cast<float>(size) + u * 1.0f};
            fillRounded(chip, 0.5f, Color{0xff, 0xff, 0xff, 224});
            DrawText(tile.game.hint.c_str(), static_cast<int>(chip.x + u * 0.7f),
                static_cast<int>(chip.y + u * 0.5f), size, palette::ink);
        }

        if (tile.focused) {
            const float thickness = std::max(u * 0.4f, 2.0f);
            const Rectangle ring{r.x - thickness / 2.0f, r.y - thickness / 2.0f,
                r.width + thickness, r.height + thickness};
            // The gradient is drawn across the tile's diagonal, in two passes, so
            // all three stops are visible.
            const float half = ring.width / 2.0f;
            const Rectangle left{ring.x, ring.y, half, ring.height};
            const Rectangle right{ring.x + half, ring.y, half, ring.height};
            fillGradient(left, palette::focusA, palette::focusB);
            fillGradient(right, palette::focusB, palette::focusC);
            // A thin white inner line, so the ring reads as raised.
            strokeRounded(r, roundness, std::max(u * 0.2f, 1.0f), Color{0xff, 0xff, 0xff, 208});
        }
    }
}

void Shell::drawFooter()
{
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
        DrawCircleV({x, static_cast<float>(baseline)},
            active ? dotRadius * 1.25f : dotRadius,
            active ? palette::dotActive : palette::dotInactive);
        x += gap;
    }

    // Corner prompts: a rounded key cap followed by the label.
    const auto hint = [this, baseline](const char* key, const char* label, int x) {
        const int size = static_cast<int>(unit() * 1.1f);
        const int keyWidth = MeasureText(key, size);
        const int capWidth = keyWidth + static_cast<int>(unit() * 1.2f);
        const int capHeight = size + static_cast<int>(unit() * 0.9f);
        const Rectangle cap{static_cast<float>(x), static_cast<float>(baseline) - capHeight,
            static_cast<float>(capWidth), static_cast<float>(capHeight)};
        fillRounded(cap, 0.4f, palette::panel);
        DrawText(key, x + static_cast<int>(unit() * 0.6f), baseline - static_cast<int>(unit() * 0.6f), size,
            palette::ink);
        DrawText(label, x + capWidth + static_cast<int>(unit() * 0.6f), baseline - static_cast<int>(unit() * 0.6f),
            size, palette::inkSoft);
    };
    hint("A", "Play", static_cast<int>(u * 1.4f));
    const int rightWidth = MeasureText("Refresh", static_cast<int>(u * 1.1f));
    hint("X", "Refresh", width_ - static_cast<int>(u * 1.4f) - rightWidth
            - static_cast<int>(u * 2.4f));
}

void Shell::drawToast()
{
    if (toast_.empty()) {
        return;
    }
    const float u = unit();
    const int size = static_cast<int>(u * 1.4f);
    const int textWidth = MeasureText(toast_.c_str(), size);
    const int padding = static_cast<int>(u * 2.0f);
    const Rectangle box{
        (static_cast<float>(width_) - textWidth - 2 * padding) / 2.0f,
        static_cast<float>(height_) - u * 8.0f,
        static_cast<float>(textWidth) + 2.0f * padding,
        static_cast<float>(size) + static_cast<float>(padding),
    };
    fillRounded(box, 0.5f, toastError_ ? Color{0xb3, 0x26, 0x1e, 240} : Color{0x2b, 0x27, 0x33, 240});
    DrawText(toast_.c_str(), static_cast<int>(box.x) + padding / 2, static_cast<int>(box.y) + padding / 3, size,
        WHITE);
}

} // namespace iideck::ui
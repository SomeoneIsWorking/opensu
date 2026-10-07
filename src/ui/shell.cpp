#include "shell.hpp"

#include <algorithm>
#include <filesystem>

#include "lucent/log.h"

namespace iideck::ui {
namespace {

namespace fs = std::filesystem;

// STOPGAP: Android reports a density and a desktop does not, so a dp is the window mapped onto
// the 853 x 480 dp canvas iiSU's renderer scales against (tb0.g).
constexpr float referenceWidthDp = 853.0f;
constexpr float referenceHeightDp = 480.0f;

// iiSU go4.java:1942: the Quick Access viewport is 3 rows by 4 columns.
constexpr int viewportRows = 3;
constexpr int viewportColumns = 4;

/// Loads a texture, returning an empty one when the file is missing or unreadable.
Texture loadArt(const fs::path& path) {
    if (path.empty() || !fs::is_regular_file(path)) {
        return Texture{};
    }
    return LoadTexture(path.string().c_str());
}

ScrollMode scrollModeFor(config::HomeMode mode) noexcept {
    return mode == config::HomeMode::WiiSu ? ScrollMode::Paged : ScrollMode::Flow;
}

bool intersectsCanvas(const Rect& rect, int width, int height) noexcept {
    // Scaled tiles grow past their cell, so the test is one tile wide on each side.
    return rect.right() + rect.width > 0.0f && rect.x - rect.width < static_cast<float>(width) &&
           rect.bottom() + rect.height > 0.0f && rect.y - rect.height < static_cast<float>(height);
}

} // namespace

Shell::Shell(int width, int height, config::HomeMode mode)
    : mode_{mode}, width_{width}, height_{height}, layout_{HomeLayoutInput{}} {
    hud_.setSize(width, height, dp());
    relayout();
}

Shell::~Shell() {
    releaseTextures();
}

bool Shell::chromeDark() noexcept {
    // iiSU gh3.q: the grid chrome is dark when the theme background's luminance is under half.
    return luminance(palette::ground) < 0.5f;
}

float Shell::dp() const noexcept {
    return std::min(static_cast<float>(width_) / referenceWidthDp,
                    static_cast<float>(height_) / referenceHeightDp);
}

HomeLayout Shell::computeLayout() const {
    return HomeLayout{HomeLayoutInput{
        .width = static_cast<float>(width_),
        .height = static_cast<float>(height_),
        .dp = dp(),
        .items = tiles_.size(),
        .mode = scrollModeFor(mode_),
        .rows = viewportRows,
        .columns = viewportColumns,
        .topInset = hud_.topInset(),
        .bottomInset = hud_.bottomInset(),
    }};
}

float Shell::sinceMs(Clock::time_point then) const noexcept {
    return std::chrono::duration<float, std::milli>(now_ - then).count();
}

float Shell::scroll() const noexcept {
    // iiSU nx2.I: Paged draws straight at the page offset; a flip is instant.
    if (layout_.mode() == ScrollMode::Paged) {
        return layout_.pageScroll(page_);
    }
    return scroller_.offset();
}

void Shell::setSize(int width, int height) {
    width_ = width;
    height_ = height;
    hud_.setSize(width, height, dp());
    relayout();
}

void Shell::setPlatforms(Platforms platforms) {
    platforms_ = std::move(platforms);
    // Tiles point into the table, so they are resolved again against the new one.
    for (Tile& tile : tiles_) {
        tile.platform = platformFor(tile.game);
    }
}

const Platform* Shell::platformFor(const library::Game& game) const {
    // A ROM belongs to a system, and the pack's console names are those systems.
    if (game.source == library::Source::Rom && !game.sourceId.empty()) {
        return platforms_.find(game.sourceId);
    }
    return platforms_.forSource(game.source);
}

void Shell::setCatalog(std::vector<library::Game> games) {
    releaseTextures();
    tiles_.clear();
    tiles_.reserve(games.size());
    for (library::Game& game : games) {
        Tile tile;
        tile.game = std::move(game);
        tile.platform = platformFor(tile.game);
        tiles_.push_back(std::move(tile));
    }
    relayout();
    page_ = 0;
    scroller_.snap(0.0f);
    focus_.reset(0, layout_.cellOf(0));
    focusAt_ = now_;
    // The catalog can arrive before the first frame, so the entrance starts on the next tick.
    entrancePending_ = true;
}

void Shell::loadArtwork() {
    for (Tile& tile : tiles_) {
        tile.portrait = loadArt(tile.game.artwork);
        tile.wide = loadArt(tile.game.artworkWide);
        tile.hasPortrait = tile.portrait.id != 0;
        tile.hasWide = tile.wide.id != 0;
    }
}

std::size_t Shell::loadedArtwork() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(tiles_, [](const Tile& tile) {
        return tile.hasPortrait;
    }));
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

void Shell::relayout() {
    layout_ = computeLayout();
    page_ = std::clamp(page_, 0, layout_.pageCount() - 1);
    if (focus_.index() >= layout_.slotCount()) {
        focus_.reset(0, layout_.cellOf(0));
    }
    if (layout_.mode() == ScrollMode::Paged) {
        page_ = layout_.pageOf(focus_.index());
        return;
    }
    scroller_.snap(layout_.scrollTarget(focus_.index(), scroller_.target(), 0));
}

void Shell::focusOn(std::size_t index, int dx) {
    focusAt_ = now_;
    if (layout_.mode() == ScrollMode::Paged) {
        page_ = layout_.pageOf(index);
        return;
    }
    scroller_.retarget(layout_.scrollTarget(index, scroller_.target(), dx));
}

void Shell::startEntrance() {
    if (entranceAt_ && sinceMs(*entranceAt_) < motion::dominoDedupeMs) {
        return;
    }
    bool any = false;
    int first = 0;
    int last = 0;
    const std::size_t slots = layout_.slotCount();
    for (std::size_t slot = 0; slot < slots; ++slot) {
        if (!intersectsCanvas(layout_.canvasRect(slot, scroll()), width_, height_)) {
            continue;
        }
        const int step = motion::dominoStep(layout_.cellOf(slot));
        first = any ? std::min(first, step) : step;
        last = any ? std::max(last, step) : step;
        any = true;
    }
    if (!any) {
        entranceAt_.reset();
        return;
    }
    entranceAt_ = now_;
    entranceFirstStep_ = first;
    entranceSpread_ = last - first;
    lucent::debug("ui", "grid entrance over {} steps, {} ms", entranceSpread_ + 1,
                  motion::dominoDurationMs(entranceSpread_));
}

bool Shell::moveFocus(Direction direction) {
    if (!focus_.move(direction, FocusGrid::of(layout_))) {
        return false;
    }
    const int dx = direction == Direction::Right ? 1 : (direction == Direction::Left ? -1 : 0);
    focusOn(focus_.index(), dx);
    return true;
}

bool Shell::movePage(int delta) {
    const int next = page_ + delta;
    if (layout_.mode() != ScrollMode::Paged || next < 0 || next >= layout_.pageCount()) {
        return false;
    }
    const std::size_t first = static_cast<std::size_t>(next) *
                              static_cast<std::size_t>(layout_.columns()) *
                              static_cast<std::size_t>(layout_.rows());
    focus_.reset(first, layout_.cellOf(first));
    focusOn(first, 0);
    return true;
}

void Shell::resetFocus() {
    focus_.reset(0, layout_.cellOf(0));
    focusOn(0, -1);
}

void Shell::pressFocused() {
    if (focus_.index() < tiles_.size()) {
        pressAt_ = now_;
        pressIndex_ = focus_.index();
    }
}

void Shell::tick(Clock::time_point now) {
    const float dt = std::chrono::duration<float, std::milli>(now - lastTick_).count();
    lastTick_ = now;
    now_ = now;
    if (entrancePending_) {
        entrancePending_ = false;
        startEntrance();
    }
    if (layout_.mode() == ScrollMode::Flow) {
        scroller_.step(dt, layout_.cellWidth() + layout_.gap(), layout_.viewportWidth(),
                       ScrollMode::Flow);
    }
    hud_.tick(now);
}

void Shell::settle() {
    float remaining = std::max(motion::focusSettleMs - sinceMs(focusAt_), 0.0f);
    if (entranceAt_) {
        remaining =
            std::max(remaining, motion::dominoDurationMs(entranceSpread_) - sinceMs(*entranceAt_));
    }
    if (pressAt_) {
        remaining = std::max(remaining, motion::pulseMs - sinceMs(*pressAt_));
    }
    now_ += std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<float, std::milli>{remaining});
    lastTick_ = now_;
    scroller_.snap(scroller_.target());
}

const library::Game* Shell::focusedGame() const {
    if (focus_.index() >= tiles_.size()) {
        return nullptr;
    }
    return &tiles_[focus_.index()].game;
}

TileVisual Shell::visualFor(std::size_t slot) const {
    TileVisual visual;
    visual.rect = layout_.canvasRect(slot, scroll());
    visual.cell = std::min(layout_.cellWidth(), layout_.cellHeight());
    visual.placeholder = slot >= tiles_.size();
    visual.dark = chromeDark();
    // iiSU ou4.q: an empty slot is a placeholder tile and takes focus like any other.
    visual.focused = slot == focus_.index();
    if (visual.focused) {
        visual.scale = motion::focusScale(sinceMs(focusAt_));
    }
    if (pressAt_ && slot == pressIndex_) {
        visual.scale *= motion::pulseScale(sinceMs(*pressAt_));
    }
    if (entranceAt_) {
        const int step = motion::dominoStep(layout_.cellOf(slot)) - entranceFirstStep_;
        const motion::Entrance entrance = motion::domino(sinceMs(*entranceAt_), step);
        visual.scale *= entrance.scale;
        visual.alpha = entrance.alpha;
    }
    visual.ringDegrees = motion::ringAngleDegrees(
        std::chrono::duration<double, std::milli>(now_.time_since_epoch()).count());
    if (!visual.placeholder) {
        const Tile& tile = tiles_[slot];
        if (tile.hasPortrait) {
            visual.art = &tile.portrait;
        } else if (tile.hasWide) {
            visual.art = &tile.wide;
        }
        visual.platform = tile.platform;
        visual.title = tile.game.title;
    }
    return visual;
}

void Shell::drawGrid() {
    // iiSU nx2.d: unfocused tiles first, the focused tile last, over its neighbours.
    const std::size_t slots = layout_.slotCount();
    const std::size_t focused = focus_.index();
    for (std::size_t slot = 0; slot < slots; ++slot) {
        if (slot == focused) {
            continue;
        }
        const TileVisual visual = visualFor(slot);
        if (intersectsCanvas(visual.rect, width_, height_)) {
            tilePainter_.paint(visual);
        }
    }
    const TileVisual visual = visualFor(focused);
    if (intersectsCanvas(visual.rect, width_, height_)) {
        tilePainter_.paint(visual);
    }
}

void Shell::draw() {
    BeginDrawing();
    hud_.drawGround();
    drawGrid();
    pillPainter_.paint(layout_.pagePill(page_), dp(), chromeDark());
    arrowPainter_.paint(layout_.pageArrows(page_), chromeDark());
    hud_.drawTopBar();
    hud_.drawHints();
    hud_.drawToast();
    EndDrawing();
}

} // namespace iideck::ui

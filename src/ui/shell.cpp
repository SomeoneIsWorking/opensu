#include "shell.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <variant>

#include "lucent/log.h"
#include "rlgl.h"
#include "tile_geometry.hpp"

namespace iideck::ui {
namespace {

namespace fs = std::filesystem;

// STOPGAP: Android reports a density and a desktop does not, so a dp is the window mapped onto
// the 853 x 480 dp canvas iiSU's renderer scales against (tb0.g).
constexpr float referenceWidthDp = 853.0f;
constexpr float referenceHeightDp = 480.0f;

// navigation.md §5.3: a console's card sits 2 dp inside its slot in an XMB and a Carousel.
constexpr float railCardInsetDp = 2.0f;
// iiSU's backdrop blur radius for glass (`homeGlassBlurRadiusDp`), as a Gaussian's sigma.
constexpr float glassBlurSigmaDp = 8.0f;

/// Loads a texture, returning an empty one when the file is missing or unreadable.
Texture loadArt(const fs::path& path) {
    if (path.empty() || !fs::is_regular_file(path)) {
        return Texture{};
    }
    return LoadTexture(path.string().c_str());
}

/// The dock's keys for the icon files: `home`, `home_selected`, `library`, `library_selected`.
std::string navKey(library::Section section, bool selected) {
    return std::string{library::key(section)} + (selected ? "_selected" : "");
}

/// The dock's stand-in letter for a section, until its icon arrives.
const char* initialOf(library::Section section) noexcept {
    return section == library::Section::Home ? "H" : "L";
}

std::string gameCount(std::size_t games) {
    return std::to_string(games) + (games == 1 ? " game" : " games");
}

TileKind kindOf(const library::ShelfItem& item) noexcept {
    if (std::holds_alternative<library::Console>(item)) {
        return TileKind::Console;
    }
    if (std::holds_alternative<library::Launcher>(item)) {
        return TileKind::Launcher;
    }
    return std::holds_alternative<library::AllGames>(item) ? TileKind::AllGames : TileKind::Game;
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
    : layout_{HomeLayoutInput{}}, mode_{mode}, width_{width}, height_{height} {
    hud_.setSize({width, height, dp()});
    relayout();
}

Shell::~Shell() {
    releaseTextures();
    if (scene_.id != 0) {
        UnloadRenderTexture(scene_);
    }
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
        .rows = gridViewport.rows,
        .columns = gridViewport.columns,
        .topInset = hud_.topInset(),
        .bottomInset = hud_.bottomInset(),
        .fillSlots = section_ == library::Section::Home,
    }};
}

float Shell::sinceMs(Clock::time_point then) const noexcept {
    return std::chrono::duration<float, std::milli>(now_ - then).count();
}

double Shell::nowMs() const noexcept {
    return std::chrono::duration<double, std::milli>(now_.time_since_epoch()).count();
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
    hud_.setSize({width, height, dp()});
    relayout();
}

const Platform* Shell::platformFor(const library::ShelfItem& item) const {
    if (const auto* console = std::get_if<library::Console>(&item)) {
        return platforms_.find(console->system);
    }
    const auto* single = std::get_if<library::Game>(&item);
    if (single == nullptr) {
        return nullptr;
    }
    const library::Game& game = *single;
    // A ROM belongs to a system, and the pack's console names are those systems.
    if (game.source == library::Source::Rom && !game.sourceId.empty()) {
        return platforms_.find(game.sourceId);
    }
    return platforms_.forSource(game.source);
}

Tile Shell::makeTile(library::ShelfItem item) const {
    Tile tile;
    tile.item = std::move(item);
    tile.platform = platformFor(tile.item);
    if (const std::optional<library::Folder> folder = library::folderOf(tile.item)) {
        tile.title = library::name(*folder);
    }
    if (const auto* console = std::get_if<library::Console>(&tile.item)) {
        tile.caption = gameCount(console->games);
    } else if (const auto* launcher = std::get_if<library::Launcher>(&tile.item)) {
        tile.caption = launcher->ready ? gameCount(launcher->games) : "Sign in";
        tile.logo = iconFor(launcher->source);
    } else if (const auto* all = std::get_if<library::AllGames>(&tile.item)) {
        tile.caption = gameCount(all->games);
    } else {
        for (const library::Source source : std::get<library::Game>(tile.item).ownedIn) {
            if (const std::optional<Icon> icon = iconFor(source)) {
                tile.stores.push_back(*icon);
            }
        }
    }
    return tile;
}

void Shell::setHeader(std::optional<library::ShelfItem> header) {
    if (header_) {
        unloadArtwork(*header_);
    }
    header_.reset();
    if (header) {
        header_ = makeTile(std::move(*header));
    }
}

void Shell::setShelf(std::vector<library::ShelfItem> items, std::size_t focus) {
    releaseTextures();
    tiles_.clear();
    tiles_.reserve(items.size());
    for (library::ShelfItem& item : items) {
        tiles_.push_back(makeTile(std::move(item)));
    }
    relayout();
    const std::size_t start = focus < tiles_.size() ? focus : 0;
    focus_.reset(start, layout_.cellOf(start));
    page_ = layout_.mode() == ScrollMode::Paged ? layout_.pageOf(start) : 0;
    scroller_.snap(layout_.scrollTarget({start, 0.0f, 0}));
    railFocus_.snap(static_cast<float>(start));
    focusAt_ = now_;
    // The catalog can arrive before the first frame, so the entrance starts on the next tick.
    entrancePending_ = true;
}

void Shell::loadTile(Tile& tile) {
    if (tile.artLoaded) {
        return;
    }
    tile.artLoaded = true;
    if (const auto* console = std::get_if<library::Console>(&tile.item)) {
        tile.portrait = loadArt(console->artwork);
        tile.hasPortrait = tile.portrait.id != 0;
        return;
    }
    const auto* single = std::get_if<library::Game>(&tile.item);
    if (single == nullptr) {
        return;
    }
    const library::Game& game = *single;
    tile.portrait = loadArt(game.artwork);
    tile.wide = loadArt(game.artworkWide);
    tile.hasPortrait = tile.portrait.id != 0;
    tile.hasWide = tile.wide.id != 0;
}

void Shell::loadArtwork() {
    glyphs_.load();
    navIcons_.load();
    for (Tile& tile : tiles_) {
        loadTile(tile);
    }
    if (header_) {
        loadTile(*header_);
    }
}

std::size_t Shell::loadedArtwork() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(tiles_, [](const Tile& tile) {
        return tile.hasPortrait;
    }));
}

void Shell::unloadArtwork(Tile& tile) {
    if (tile.hasPortrait) {
        UnloadTexture(tile.portrait);
        tile.hasPortrait = false;
    }
    if (tile.hasWide) {
        UnloadTexture(tile.wide);
        tile.hasWide = false;
    }
    tile.artLoaded = false;
}

void Shell::setArtwork(std::string_view gameId, const std::filesystem::path& artwork) {
    for (Tile& tile : tiles_) {
        auto* game = std::get_if<library::Game>(&tile.item);
        if (game != nullptr && game->id == gameId) {
            game->artwork = artwork;
            unloadArtwork(tile);
        }
    }
}

void Shell::setConsoleArtwork(std::string_view system, const std::filesystem::path& artwork) {
    const auto give = [&](Tile& tile) {
        auto* console = std::get_if<library::Console>(&tile.item);
        if (console != nullptr && console->system == system) {
            console->artwork = artwork;
            unloadArtwork(tile);
        }
    };
    for (Tile& tile : tiles_) {
        give(tile);
    }
    if (header_) {
        give(*header_);
    }
}

void Shell::setGlyph(std::string_view system, const std::filesystem::path& glyph) {
    glyphs_.set(system, glyph);
}

void Shell::setNavIcon(library::Section section, bool selected, const std::filesystem::path& file) {
    navIcons_.set(navKey(section, selected), file);
    if (section == library::Section::Library && !selected) {
        railPainter_.setSectionIcon(file);
    }
}

void Shell::setSection(library::Section section, bool revealDock) {
    section_ = section;
    const double now = nowMs();
    dockVisibility_.setPinned(section == library::Section::Home, now);
    if (revealDock) {
        dockVisibility_.reveal(now);
    }
    for (std::size_t i = 0; i < library::allSections.size(); ++i) {
        iconPops_[i].select(library::allSections[i] == section, now);
    }
    hud_.setStartMenu(section == library::Section::Library);
    relayout();
}

void Shell::setLibraryMode(library::LibraryMode mode) {
    libraryMode_ = mode;
    entrancePending_ = true;
    railFocus_.snap(static_cast<float>(focus_.index()));
    relayout();
}

void Shell::releaseTextures() {
    for (Tile& tile : tiles_) {
        unloadArtwork(tile);
    }
    if (header_) {
        unloadArtwork(*header_);
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
    scroller_.snap(layout_.scrollTarget({focus_.index(), scroller_.target(), 0}));
}

void Shell::focusOn(std::size_t index, int dx) {
    focusAt_ = now_;
    if (layout_.mode() == ScrollMode::Paged) {
        page_ = layout_.pageOf(index);
        return;
    }
    scroller_.retarget(layout_.scrollTarget({index, scroller_.target(), dx}));
}

void Shell::startEntrance() {
    // The rails fade in rather than rise (navigation.md §5.3).
    if (presentation() != Presentation::Grid) {
        entranceAt_ = now_;
        entranceFirstStep_ = 0;
        entranceSpread_ = 0;
        return;
    }
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

bool Shell::moveRail(Direction direction) {
    const bool column = presentation() == Presentation::Xmb;
    int step = 0;
    if (column) {
        step = direction == Direction::Down ? 1 : (direction == Direction::Up ? -1 : 0);
    } else {
        step = direction == Direction::Right ? 1 : (direction == Direction::Left ? -1 : 0);
    }
    const auto last = static_cast<int>(tiles_.size()) - 1;
    const int next = std::clamp(static_cast<int>(focus_.index()) + step, 0, std::max(last, 0));
    if (step == 0 || next == static_cast<int>(focus_.index())) {
        return false;
    }
    const auto index = static_cast<std::size_t>(next);
    focus_.reset(index, layout_.cellOf(index));
    railFocus_.retarget(static_cast<float>(index));
    focusAt_ = now_;
    return true;
}

bool Shell::moveFocus(Direction direction) {
    if (presentation() != Presentation::Grid) {
        return moveRail(direction);
    }
    if (!focus_.move(direction, FocusGrid::of(layout_))) {
        return false;
    }
    const int dx = direction == Direction::Right ? 1 : (direction == Direction::Left ? -1 : 0);
    focusOn(focus_.index(), dx);
    return true;
}

void Shell::resetFocus() {
    focus_.reset(0, layout_.cellOf(0));
    railFocus_.retarget(0.0f);
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
    railFocus_.step(dt);
    if (layout_.mode() == ScrollMode::Flow) {
        scroller_.step(dt, layout_.cellWidth() + layout_.gap(), layout_.viewportWidth(),
                       ScrollMode::Flow);
    }
    hud_.tick(now);
}

void Shell::settle() {
    float remaining = std::max(motion::focusSettleMs - sinceMs(focusAt_), 0.0f);
    if (entranceAt_) {
        const float length = presentation() == Presentation::Grid
                                 ? motion::dominoDurationMs(entranceSpread_)
                                 : motion::railEntranceMs;
        remaining = std::max(remaining, length - sinceMs(*entranceAt_));
    }
    if (pressAt_) {
        remaining = std::max(remaining, motion::pulseMs - sinceMs(*pressAt_));
    }
    now_ += std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<float, std::milli>{remaining});
    lastTick_ = now_;
    scroller_.snap(scroller_.target());
    railFocus_.snap(static_cast<float>(focus_.index()));
}

const library::Game* Shell::focusedGame() const {
    if (focus_.index() >= tiles_.size()) {
        return nullptr;
    }
    return std::get_if<library::Game>(&tiles_[focus_.index()].item);
}

std::optional<library::Folder> Shell::focusedFolder() const {
    if (focus_.index() >= tiles_.size()) {
        return std::nullopt;
    }
    return library::folderOf(tiles_[focus_.index()].item);
}

std::string Shell::focusedTitle() const {
    if (const library::Game* game = focusedGame()) {
        return game->title;
    }
    if (const std::optional<library::Folder> folder = focusedFolder()) {
        return library::name(*folder);
    }
    return {};
}

std::string Shell::pillTitle() const {
    return presentation() == Presentation::Grid ? focusedTitle() : std::string{};
}

void Shell::fillContent(TileVisual& visual, const Tile& tile) const {
    if (tile.hasPortrait) {
        visual.art = &tile.portrait;
    } else if (tile.hasWide) {
        visual.art = &tile.wide;
    }
    visual.platform = tile.platform;
    if (const auto* game = std::get_if<library::Game>(&tile.item)) {
        visual.title = game->title;
        visual.stores = tile.stores;
        if (game->source == library::Source::Rom) {
            visual.glyph = glyphs_.find(game->sourceId);
        }
    } else {
        visual.title = tile.title;
        visual.caption = tile.caption;
        visual.logo = tile.logo;
        visual.kind = kindOf(tile.item);
    }
}

TileVisual Shell::visualFor(std::size_t slot, const Rect& rect) const {
    const bool grid = presentation() == Presentation::Grid;
    TileVisual visual;
    visual.rect = rect;
    visual.cell = grid ? std::min(layout_.cellWidth(), layout_.cellHeight())
                       : std::min(rect.width, rect.height);
    visual.placeholder = slot >= tiles_.size();
    visual.dark = chromeDark();
    // An empty slot, which only Home has, is a placeholder tile and takes focus (iiSU ou4.q).
    visual.focused = slot == focus_.index();
    // iiSU w70: the focus scale is the grid's; an XMB holds its focused tile at 1.
    if (visual.focused && grid) {
        visual.scale = motion::focusScale(sinceMs(focusAt_));
    }
    if (pressAt_ && slot == pressIndex_) {
        visual.scale *= motion::pulseScale(sinceMs(*pressAt_));
    }
    if (entranceAt_ && grid) {
        const int step = motion::dominoStep(layout_.cellOf(slot)) - entranceFirstStep_;
        const motion::Entrance entrance = motion::domino(sinceMs(*entranceAt_), step);
        visual.scale *= entrance.scale;
        visual.alpha = entrance.alpha;
    } else if (entranceAt_) {
        const float distance = std::round(std::abs(static_cast<float>(slot) - railFocus_.value()));
        visual.alpha = railAlpha(static_cast<int>(distance));
    }
    visual.selectionRing = grid;
    visual.ringDegrees = motion::ringAngleDegrees(nowMs());
    if (!visual.placeholder) {
        fillContent(visual, tiles_[slot]);
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
        const TileVisual visual = visualFor(slot, layout_.canvasRect(slot, scroll()));
        if (intersectsCanvas(visual.rect, width_, height_)) {
            tilePainter_.paint(visual);
        }
    }
    const TileVisual visual = visualFor(focused, layout_.canvasRect(focused, scroll()));
    if (intersectsCanvas(visual.rect, width_, height_)) {
        tilePainter_.paint(visual);
    }
}

float Shell::aspectOf(const Tile& tile) const noexcept {
    const Texture* art = tile.hasPortrait ? &tile.portrait : (tile.hasWide ? &tile.wide : nullptr);
    return art != nullptr && art->height > 0
               ? static_cast<float>(art->width) / static_cast<float>(art->height)
               : 1.0f;
}

RailInput Shell::railInput() const {
    RailInput input{
        static_cast<float>(width_), static_cast<float>(height_), {}, railFocus_.value(), 9, dp()};
    input.aspects.reserve(tiles_.size());
    for (const Tile& tile : tiles_) {
        input.aspects.push_back(aspectOf(tile));
    }
    return input;
}

Rect Shell::railSlot(const Tile& tile, const Rect& rect) const noexcept {
    // The frame's edge is what fills a slot, not the chrome around it.
    const float inset =
        std::holds_alternative<library::Game>(tile.item) ? 0.0f : railCardInsetDp * dp();
    return outerForContent(Rect{rect.x + inset, rect.y + inset,
                                std::max(rect.width - 2.0f * inset, 0.0f),
                                std::max(rect.height - 2.0f * inset, 0.0f)});
}

float Shell::railAlpha(int distance) const noexcept {
    return entranceAt_
               ? motion::railEntranceAlpha(motion::RailEntrance{sinceMs(*entranceAt_), distance})
               : 1.0f;
}

void Shell::drawRail() {
    const bool xmb = presentation() == Presentation::Xmb;
    const RailInput input = railInput();
    const XmbLayout column{input};
    const CarouselLayout row{input};
    const std::vector<Rect>& rects = xmb ? column.rects() : row.rects();
    // The tiles nearest the focus are drawn last, the focused one over its neighbours.
    std::vector<std::size_t> order(rects.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    const float focus = railFocus_.value();
    std::ranges::sort(order, [focus](std::size_t a, std::size_t b) {
        return std::abs(static_cast<float>(a) - focus) > std::abs(static_cast<float>(b) - focus);
    });
    for (const std::size_t slot : order) {
        const Rect rect = railSlot(tiles_[slot], rects[slot]);
        if (intersectsCanvas(rect, width_, height_)) {
            tilePainter_.paint(visualFor(slot, rect));
        }
    }
    if (tiles_.empty()) {
        return;
    }
    const std::size_t focused = std::min(focus_.index(), tiles_.size() - 1);
    const std::string title = focusedTitle();
    const RailStyle style{chromeDark(), railAlpha(1)};
    if (!xmb) {
        railPainter_.paintTitle(row, title, style);
        if (header_) {
            railPainter_.paintRowMarker(row, style);
        }
        return;
    }
    railPainter_.paintTitle(column, title, style);
    const RailStyle columnStyle{style.dark, railAlpha(0)};
    if (header_) {
        TileVisual card;
        card.rect = outerForContent(column.headerCard());
        card.cell = column.headerCard().width;
        card.dark = style.dark;
        card.selectionRing = false;
        card.alpha = columnStyle.alpha;
        fillContent(card, *header_);
        tilePainter_.paint(card);
        railPainter_.paintColumnMarker(column, columnStyle);
        return;
    }
    Focused source;
    source.platform = tiles_[focused].platform;
    if (const auto* console = std::get_if<library::Console>(&tiles_[focused].item)) {
        source.key = console->system;
        source.card = console->artwork;
    }
    railPainter_.paintColumn(column, source, columnStyle);
}

void Shell::ensureScene() {
    if (scene_.id != 0 && scene_.texture.width == width_ && scene_.texture.height == height_) {
        return;
    }
    if (scene_.id != 0) {
        UnloadRenderTexture(scene_);
    }
    scene_ = LoadRenderTexture(width_, height_);
}

Shell::DockFrame Shell::dockFrame(float width, float height) const {
    const float pixelsPerDp = dp();
    const std::vector<float> widths{plainItem, wideItem};
    const DockMetrics metrics{width / pixelsPerDp, height / pixelsPerDp, widths};
    return DockFrame{metrics, layoutDock(metrics, width, height, pixelsPerDp),
                     DockStyle{chromeDark(), pixelsPerDp, dockVisibility_.progress(nowMs())}};
}

std::optional<library::Section> Shell::pointDock(std::optional<Vector2> point) {
    dockHover_.reset();
    if (!point || inGame_) {
        return std::nullopt;
    }
    const DockFrame frame = dockFrame(static_cast<float>(width_), static_cast<float>(height_));
    if (onRestingDock(frame.layout, point->x, point->y)) {
        dockVisibility_.reveal(nowMs());
    }
    const float slide = DockPainter::barRect(frame.layout, frame.style).y - frame.layout.bar.y;
    if (const std::optional<std::size_t> item =
            dockItemAt(frame.layout, slide, point->x, point->y)) {
        dockHover_ = library::allSections[*item];
    }
    return dockHover_;
}

void Shell::drawDock(const DockFrame& frame, float frameHeight) {
    const double now = nowMs();
    blur_.draw(DockPainter::barRect(frame.layout, frame.style),
               BackdropBlur::Placement{frameHeight, std::clamp(frame.style.progress, 0.0f, 1.0f)});
    std::array<DockIcon, library::allSections.size()> icons;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const library::Section section = library::allSections[i];
        icons[i] = DockIcon{navIcons_.find(navKey(section, section == section_)),
                            iconPops_[i].scale(now), initialOf(section), dockHover_ == section};
    }
    dockPainter_.paint(frame.layout, frame.metrics, icons, frame.style);
}

void Shell::drawScene() {
    hud_.drawGround();
    if (presentation() == Presentation::Grid) {
        drawGrid();
        pillPainter_.paint(layout_.pagePill(page_), dp(), chromeDark());
        arrowPainter_.paint(layout_.pageArrows(page_), chromeDark());
    } else {
        drawRail();
    }
    hud_.drawTopBar();
    hud_.drawHints();
}

void Shell::draw(const RenderTexture2D* target) {
    // Textures need the GL context, which only the drawing thread has.
    loadArtwork();
    const auto frameWidth = static_cast<float>(target != nullptr ? target->texture.width : width_);
    const auto frameHeight =
        static_cast<float>(target != nullptr ? target->texture.height : height_);
    BeginDrawing();
    // Alpha accumulates as over-compositing does, so the window's own alpha is what Gamescope
    // and an ARGB visual blend with; raylib's BLEND_ALPHA would square it.
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
                              RL_FUNC_ADD, RL_FUNC_ADD);
    if (inGame_) {
        if (target != nullptr) {
            BeginTextureMode(*target);
        }
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        ClearBackground(BLANK);
        gameMenuPainter_.paint(gameMenu_, frameWidth, frameHeight, dp());
        EndBlendMode();
        if (target != nullptr) {
            EndTextureMode();
        }
        EndDrawing();
        return;
    }

    // The scene goes to its own texture so the dock's glass can blur what is behind it.
    ensureScene();
    BeginTextureMode(scene_);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    drawScene();
    EndBlendMode();
    EndTextureMode();
    const DockFrame dock = dockFrame(frameWidth, frameHeight);
    blur_.prepare(scene_.texture, DockPainter::barRect(dock.layout, dock.style),
                  glassBlurSigmaDp * dp());

    if (target != nullptr) {
        BeginTextureMode(*target);
    }
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawTextureRec(scene_.texture,
                   Rectangle{0.0f, 0.0f, static_cast<float>(scene_.texture.width),
                             -static_cast<float>(scene_.texture.height)},
                   Vector2{0.0f, 0.0f}, WHITE);
    drawDock(dock, frameHeight);
    launchPanelPainter_.paint(launchPanel_, Vector2{frameWidth, frameHeight}, dp(),
                              std::chrono::duration<double>(now_.time_since_epoch()).count());
    chooserPainter_.paint(chooser_, Vector2{frameWidth, frameHeight}, dp());
    hud_.drawToast();
    EndBlendMode();
    if (target != nullptr) {
        EndTextureMode();
    }
    EndDrawing();
}

} // namespace iideck::ui

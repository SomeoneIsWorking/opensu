#include "shell.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <type_traits>
#include <variant>

#include "input/keyboard_bindings.hpp"
#include "lucent/log.h"
#include "rlgl.h"
#include "tile_geometry.hpp"

namespace opensu::ui {
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
    artwork_.beginShelf(tiles_, header_);
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
                    static_cast<float>(height_) / referenceHeightDp) *
           static_cast<float>(uiScale_) / 100.0f;
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
        .iconLevel = section_ == library::Section::Library ? iconSize_ : defaultIconLevel,
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

Tile Shell::makeTile(library::ShelfItem item) const {
    Tile tile;
    tile.item = std::move(item);
    tile.platform = platforms_.forItem(tile.item);
    if (const std::optional<library::Folder> folder = library::folderOf(tile.item)) {
        tile.title = library::name(*folder);
    }
    if (const auto* console = std::get_if<library::Console>(&tile.item)) {
        tile.caption = gameCount(console->games);
    } else if (const auto* launcher = std::get_if<library::Launcher>(&tile.item)) {
        tile.caption = launcher->loading
                           ? "Loading"
                           : (launcher->ready ? gameCount(launcher->games) : "Sign in");
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
        artwork_.unload(*header_);
    }
    header_.reset();
    if (header) {
        header_ = makeTile(std::move(*header));
        header_->ticket = artwork_.ticket();
    }
}

void Shell::setShelf(std::vector<library::ShelfItem> items, std::size_t focus) {
    artwork_.beginShelf(tiles_, header_);
    tiles_.clear();
    tiles_.reserve(items.size());
    gameTiles_.clear();
    for (library::ShelfItem& item : items) {
        if (const auto* game = std::get_if<library::Game>(&item)) {
            gameTiles_.emplace(game->id, tiles_.size());
        }
        tiles_.push_back(makeTile(std::move(item)));
        tiles_.back().ticket = artwork_.ticket();
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

SlotRange Shell::artWindow() const {
    SlotRange window;
    if (presentation() == Presentation::Grid) {
        window = layout_.visibleSlots(scroll(), static_cast<float>(width_),
                                      layout_.viewportWidth() * 0.5f);
    } else {
        const RailInput input = railInput();
        if (presentation() == Presentation::Xmb) {
            const XmbLayout column{input};
            window = SlotRange{column.first(), column.first() + column.rects().size()};
        } else {
            const CarouselLayout row{input};
            window = SlotRange{row.first(), row.first() + row.rects().size()};
        }
    }
    window.last = std::min(window.last, tiles_.size());
    window.first = std::min(window.first, window.last);
    return window;
}

void Shell::loadArtwork(std::size_t uploads) {
    glyphs_.load();
    navIcons_.load();
    artwork_.load(tiles_, header_, artWindow(), uploads);
}

void Shell::loadArtworkNow() {
    loadArtwork(std::numeric_limits<std::size_t>::max());
    while (!artwork_.idle()) {
        artwork_.waitDecoded();
        loadArtwork(std::numeric_limits<std::size_t>::max());
    }
}

std::size_t Shell::loadedArtwork() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(tiles_, [](const Tile& tile) {
        return tile.hasPortrait;
    }));
}

void Shell::setArtwork(std::string_view gameId, const std::filesystem::path& artwork) {
    const auto found = gameTiles_.find(std::string{gameId});
    if (found == gameTiles_.end()) {
        return;
    }
    Tile& tile = tiles_[found->second];
    if (auto* game = std::get_if<library::Game>(&tile.item)) {
        game->artwork = artwork;
        artwork_.unload(tile);
    }
    tile.artDownloading = false;
}

void Shell::setArtworkDownloading(const std::vector<std::string>& gameIds) {
    for (Tile& tile : tiles_) {
        tile.artDownloading = false;
    }
    for (const std::string& id : gameIds) {
        if (const auto found = gameTiles_.find(id); found != gameTiles_.end()) {
            tiles_[found->second].artDownloading = true;
        }
    }
}

std::vector<std::string> Shell::downloadingOnScreen() const {
    std::vector<std::string> ids;
    const SlotRange window = artWindow();
    for (std::size_t index = window.first; index < window.last; ++index) {
        const auto* game = std::get_if<library::Game>(&tiles_[index].item);
        if (game != nullptr && tiles_[index].artDownloading) {
            ids.push_back(game->id);
        }
    }
    return ids;
}

void Shell::artworkSettled(std::string_view gameId) {
    if (const auto found = gameTiles_.find(std::string{gameId}); found != gameTiles_.end()) {
        tiles_[found->second].artDownloading = false;
    }
}

void Shell::setConsoleArtwork(std::string_view system, const std::filesystem::path& artwork) {
    const auto give = [&](Tile& tile) {
        auto* console = std::get_if<library::Console>(&tile.item);
        if (console != nullptr && console->system == system) {
            console->artwork = artwork;
            artwork_.unload(tile);
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
    dockVisibility_.setPinned(dockPinned(section, pinLibraryDock_), now);
    if (revealDock) {
        dockVisibility_.reveal(now);
    }
    for (std::size_t i = 0; i < library::allSections.size(); ++i) {
        iconPops_[i].select(library::allSections[i] == section, now);
    }
    relayout();
}

void Shell::setPinLibraryDock(bool pinned) {
    pinLibraryDock_ = pinned;
    dockVisibility_.setPinned(dockPinned(section_, pinLibraryDock_), nowMs());
}

void Shell::setLibraryMode(library::LibraryMode mode) {
    libraryMode_ = mode;
    entrancePending_ = true;
    railFocus_.snap(static_cast<float>(focus_.index()));
    relayout();
}

void Shell::setIconSize(int level) {
    iconSize_ = clampIconLevel(level);
    relayout();
}

void Shell::setUiScale(int percent) {
    if (percent != uiScale_) {
        uiScale_ = percent;
        hud_.setSize({width_, height_, dp()});
        relayout();
    }
}

void Shell::setHomeMode(config::HomeMode mode) {
    if (mode != mode_) {
        mode_ = mode;
        relayout();
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
    const SlotRange seen = layout_.visibleSlots(scroll(), static_cast<float>(width_));
    for (std::size_t slot = seen.first; slot < seen.last; ++slot) {
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

void Shell::followFade(PanelFade& fade, bool open, bool& wasOpen) const {
    if (open == wasOpen) {
        return;
    }
    wasOpen = open;
    if (open) {
        fade.show(now_);
    } else {
        fade.hide(now_);
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
    // A panel's fade starts when its state says it opened or closed.
    followFade(searchFade_, search_.isOpen(), searchWasOpen_);
    followFade(contextFade_, context_.isOpen(), contextWasOpen_);
    followFade(detailsFade_, details_.isOpen(), detailsWasOpen_);
    settings_.tick(now_);
    session_.tick(now_);
    devices_.tick(now_);
    volumeOsd_.tick(now_);
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

const library::ShelfItem* Shell::focusedItem() const {
    return focus_.index() < tiles_.size() ? &tiles_[focus_.index()].item : nullptr;
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
    // The grid names its focused tile; a page that replaces the grid names itself in the trail.
    const bool page = details_.isOpen() || settings_.anyOpen() || devices_.isOpen();
    return presentation() == Presentation::Grid && !page ? focusedTitle() : std::string{};
}

void Shell::fillContent(TileVisual& visual, const Tile& tile) const {
    if (tile.hasPortrait) {
        visual.art = &tile.portrait;
    } else if (tile.hasWide) {
        visual.art = &tile.wide;
    }
    visual.platform = tile.platform;
    visual.loading = visual.art == nullptr && (tile.artDownloading || tile.artRequested);
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
    visual.seconds = nowMs() / 1000.0;
    if (!visual.placeholder) {
        fillContent(visual, tiles_[slot]);
    }
    return visual;
}

void Shell::drawGrid() {
    // iiSU nx2.d: unfocused tiles first, the focused tile last, over its neighbours.
    const SlotRange seen = layout_.visibleSlots(scroll(), static_cast<float>(width_));
    const std::size_t focused = focus_.index();
    for (std::size_t slot = seen.first; slot < seen.last; ++slot) {
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
    return RailInput{static_cast<float>(width_),
                     static_cast<float>(height_),
                     tiles_.size(),
                     [this](std::size_t index) {
                         return aspectOf(tiles_[index]);
                     },
                     railFocus_.value(),
                     iconSize_,
                     dp()};
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

std::vector<Rect> Shell::railSlots(const std::vector<Rect>& rects, std::size_t first) const {
    std::vector<Rect> slots;
    slots.reserve(rects.size());
    for (std::size_t i = 0; i < rects.size(); ++i) {
        slots.push_back(railSlot(tiles_[first + i], rects[i]));
    }
    return slots;
}

void Shell::drawRail() {
    const bool xmb = presentation() == Presentation::Xmb;
    const RailInput input = railInput();
    const XmbLayout column{input};
    const CarouselLayout row{input};
    const std::size_t first = xmb ? column.first() : row.first();
    const std::vector<Rect> rects = railSlots(xmb ? column.rects() : row.rects(), first);
    // The tiles nearest the focus are drawn last, the focused one over its neighbours.
    std::vector<std::size_t> order(rects.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    const float focus = railFocus_.value() - static_cast<float>(first);
    std::ranges::sort(order, [focus](std::size_t a, std::size_t b) {
        return std::abs(static_cast<float>(a) - focus) > std::abs(static_cast<float>(b) - focus);
    });
    for (const std::size_t slot : order) {
        if (intersectsCanvas(rects[slot], width_, height_)) {
            tilePainter_.paint(visualFor(first + slot, rects[slot]));
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

DetailsArt Shell::detailsArt() const {
    if (focus_.index() >= tiles_.size()) {
        return {};
    }
    const Tile& tile = tiles_[focus_.index()];
    return DetailsArt::of(tile.hasPortrait ? &tile.portrait : nullptr,
                          tile.hasWide ? &tile.wide : nullptr);
}

PointerTarget Shell::pointAt(std::optional<Vector2> point) {
    dockHover_.reset();
    hud_.setLauncherHover(std::nullopt);
    hud_.setCrumbHover(std::nullopt);
    if (!point) {
        return {};
    }
    if (!inGame_ && !launchPanel_.isOpen() &&
        (!panelOpen() || details_.isOpen() || settings_.onlyPage() || devices_.isOpen()) &&
        !guide_.anyOpen()) {
        if (const std::optional<std::size_t> crumb = hud_.crumbAt(point->x, point->y)) {
            hud_.setCrumbHover(crumb);
            return OnCrumb{*crumb};
        }
    }
    if (inGame_ || launchPanel_.isOpen() || panelOpen()) {
        return pointAtModal(*point);
    }
    if (const std::optional<library::Source> launcher = hud_.launcherAt(point->x, point->y)) {
        hud_.setLauncherHover(launcher);
        return OnLauncher{*launcher};
    }
    const DockFrame frame = dockFrame(static_cast<float>(width_), static_cast<float>(height_));
    if (onRestingDock(frame.layout, point->x, point->y)) {
        dockVisibility_.reveal(nowMs());
    }
    const float slide = DockPainter::barRect(frame.layout, frame.style).y - frame.layout.bar.y;
    if (const std::optional<std::size_t> item =
            dockItemAt(frame.layout, slide, point->x, point->y)) {
        dockHover_ = library::allSections[*item];
        return OnDock{*dockHover_};
    }
    return pointAtHome(*point);
}

PointerTarget Shell::pointAtModal(Vector2 point) const {
    const auto width = static_cast<float>(width_);
    const auto height = static_cast<float>(height_);
    if (session_.anyOpen()) {
        return session_.pointAt(point, Vector2{width, height}, dp());
    }
    if (guide_.anyOpen()) {
        return guide_.pointAt(point, Vector2{width, height}, dp());
    }
    if (inGame_ && !settings_.anyOpen() && !devices_.isOpen()) {
        return {};
    }
    if (launchPanel_.isOpen()) {
        const PanelLayout panel =
            launchPanelPainter_.layout(launchPanel_, Vector2{width, height}, dp());
        if (const std::optional<std::size_t> hint = panel.hintAt(point.x, point.y)) {
            const gamepad::Button button = input::buttonOfGlyph(launchPanel_.hints()[*hint].button);
            if (button != gamepad::Button::None) {
                return OnPanelButton{button};
            }
        }
        return {};
    }
    const Rect frame{0.0f, 0.0f, width, height};
    if (settings_.anyOpen()) {
        return settings_.pointAt(point, Vector2{width, height}, dp(), hud_.topInset(),
                                 hud_.bottomInset());
    }
    if (devices_.isOpen()) {
        return devices_.pointAt(point, Vector2{width, height}, dp(), hud_.topInset(),
                                hud_.bottomInset());
    }
    if (details_.isOpen()) {
        if (const std::optional<std::size_t> button = detailsLayout().buttonAt(point.x, point.y)) {
            return OnDetailsButton{*button};
        }
        return {};
    }
    if (context_.isOpen()) {
        const ContextLayout menu = context_.layout(frame, dp());
        if (const std::optional<std::size_t> row = menu.itemAt(point.x, point.y)) {
            return OnContextItem{*row};
        }
        return menu.contains(point.x, point.y) ? PointerTarget{} : OnBackdrop{};
    }
    if (search_.isOpen()) {
        const SearchLayout panel = layoutSearch(frame, dp());
        if (const std::optional<std::size_t> key = panel.keyAt(point.x, point.y)) {
            return OnSearchKey{*key};
        }
        if (const std::optional<std::size_t> row = panel.resultAt(point.x, point.y)) {
            const std::size_t result = search_.firstListed() + *row;
            return result < search_.results().size() ? PointerTarget{OnSearchResult{result}}
                                                     : PointerTarget{};
        }
        return {};
    }
    const ChooserLayout picker = chooser_.layout(frame, dp());
    if (const std::optional<int> level = picker.iconSizeAt(point.x, point.y)) {
        return OnIconSize{*level};
    }
    if (const std::optional<ChooserRow> row = picker.rowAt(point.x, point.y)) {
        return OnChooserRow{*row};
    }
    if (const std::optional<library::LibraryMode> card = picker.cardAt(point.x, point.y)) {
        return OnLayoutCard{*card};
    }
    return {};
}

PointerTarget Shell::pointAtHome(Vector2 point) const {
    if (presentation() != Presentation::Grid) {
        const RailInput input = railInput();
        const bool xmb = presentation() == Presentation::Xmb;
        const XmbLayout column{input};
        const CarouselLayout row{input};
        const std::size_t first = xmb ? column.first() : row.first();
        const std::vector<Rect> slots = railSlots(xmb ? column.rects() : row.rects(), first);
        if (const std::optional<std::size_t> tile =
                railTileAt(slots, first, railFocus_.value(), point.x, point.y)) {
            return OnTile{*tile};
        }
        return {};
    }
    if (const std::optional<int> page = layout_.pageAt(page_, point.x, point.y)) {
        return OnPage{*page};
    }
    if (const std::optional<std::size_t> slot = layout_.slotAt(scroll(), point.x, point.y)) {
        return OnTile{*slot};
    }
    return {};
}

bool Shell::focusTile(std::size_t index) {
    const bool rail = presentation() != Presentation::Grid;
    if (index == focus_.index() || index >= (rail ? tiles_.size() : layout_.slotCount())) {
        return false;
    }
    const GridCell from = layout_.cellOf(focus_.index());
    const GridCell to = layout_.cellOf(index);
    focus_.reset(index, to);
    if (rail) {
        railFocus_.retarget(static_cast<float>(index));
        focusAt_ = now_;
    } else {
        focusOn(index, to.left > from.left ? 1 : (to.left < from.left ? -1 : 0));
    }
    return true;
}

bool Shell::focusPage(int page) {
    if (layout_.mode() != ScrollMode::Paged || page == page_ || page < 0 ||
        page >= layout_.pageCount()) {
        return false;
    }
    return focusTile(layout_.slotOnPage(page, focus_.rememberedRow(), page < page_));
}

bool Shell::focusTarget(const PointerTarget& target) {
    // What has no focus to move (no target, the dock, a panel's hint, a badge, a crumb, a backdrop)
    // does not move it.
    return std::visit(
        [this](const auto& at) {
            using Target = std::decay_t<decltype(at)>;
            if constexpr (std::is_same_v<Target, OnDetailsButton>) {
                return details_.focusButton(at.index);
            } else if constexpr (std::is_same_v<Target, OnChooserRow>) {
                return chooser_.focusRow(at.row);
            } else if constexpr (std::is_same_v<Target, OnIconSize>) {
                return chooser_.focusRow(ChooserRow::IconSize);
            } else if constexpr (std::is_same_v<Target, OnDialogButton>) {
                return session_.focusTarget(PointerTarget{at});
            } else if constexpr (std::is_same_v<Target, OnSearchKey>) {
                if (session_.password().isOpen()) {
                    return session_.focusTarget(PointerTarget{at});
                }
                return settings_.entry().isOpen() ? settings_.entry().focusKey(at.index)
                                                  : search_.focusKey(at.index);
            } else if constexpr (std::is_same_v<Target, OnSettingsCategory> ||
                                 std::is_same_v<Target, OnSettingsRow> ||
                                 std::is_same_v<Target, OnSettingsSlider> ||
                                 std::is_same_v<Target, OnFolderEntry>) {
                return devices_.isOpen() ? devices_.focusTarget(at) : settings_.focusTarget(at);
            } else if constexpr (std::is_same_v<Target, OnGuideEntry> ||
                                 std::is_same_v<Target, OnGuidePower> ||
                                 std::is_same_v<Target, OnQuickRow> ||
                                 std::is_same_v<Target, OnQuickSlider>) {
                return guide_.focusTarget(at);
            } else if constexpr (std::is_same_v<Target, OnSearchResult>) {
                return search_.focusResult(at.index);
            } else if constexpr (std::is_same_v<Target, OnContextItem>) {
                return context_.focusItem(at.index);
            } else if constexpr (std::is_same_v<Target, OnTile>) {
                return focusTile(at.index);
            } else if constexpr (std::is_same_v<Target, OnPage>) {
                return focusPage(at.page);
            } else if constexpr (std::is_same_v<Target, OnLayoutCard>) {
                return chooser_.focus(at.mode);
            } else {
                return false;
            }
        },
        target);
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
    const bool settingsShown = settings_.pageVisible(now_) || devices_.visible(now_);
    const bool detailsShown = detailsFade_.visible(now_);
    if (presentation() == Presentation::Grid) {
        drawGrid();
        if (!detailsShown && !settingsShown) {
            pillPainter_.paint(layout_.pagePill(page_), dp(), chromeDark());
            arrowPainter_.paint(layout_.pageArrows(page_), chromeDark());
        }
    } else {
        drawRail();
    }
    if (detailsShown) {
        DetailsPagePainter::paint(details_, detailsLayout(), detailsArt(),
                                  Vector2{static_cast<float>(width_), static_cast<float>(height_)},
                                  dp(), detailsFade_.alpha(now_));
    }
    drawPages(Vector2{static_cast<float>(width_), static_cast<float>(height_)});
    hud_.drawTopBar();
    hud_.drawHints();
}

void Shell::drawPages(Vector2 size) {
    settings_.drawPage(size, dp(), hud_.topInset(), hud_.bottomInset(), now_);
    devices_.draw(size, dp(), hud_.topInset(), hud_.bottomInset(), now_);
}

void Shell::drawOverGame(Vector2 size) {
    // Only what the Guide menus open is drawn; the game shows through everywhere else.
    drawPages(size);
    if (settings_.pageVisible(now_) || devices_.visible(now_)) {
        hud_.drawTopBar();
        hud_.drawHints();
    }
    const double seconds = std::chrono::duration<double>(now_.time_since_epoch()).count();
    settings_.drawOverlays(size, dp(), now_, seconds);
    guide_.draw(size, dp());
    session_.draw(size, dp(), now_, seconds);
    hud_.drawToast();
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
        drawOverGame(Vector2{frameWidth, frameHeight});
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
    if (!detailsFade_.visible(now_) && !settings_.pageVisible(now_) && !devices_.visible(now_)) {
        drawDock(dock, frameHeight);
    }
    launchPanelPainter_.paint(launchPanel_, Vector2{frameWidth, frameHeight}, dp(),
                              std::chrono::duration<double>(now_.time_since_epoch()).count());
    chooserPainter_.paint(chooser_, Vector2{frameWidth, frameHeight}, dp());
    const double seconds = std::chrono::duration<double>(now_.time_since_epoch()).count();
    searchPainter_.paint(search_, Vector2{frameWidth, frameHeight}, dp(), searchFade_.look(now_),
                         seconds);
    contextPainter_.paint(context_, Vector2{frameWidth, frameHeight}, dp(),
                          contextFade_.look(now_));
    settings_.drawOverlays(Vector2{frameWidth, frameHeight}, dp(), now_, seconds);
    guide_.draw(Vector2{frameWidth, frameHeight}, dp());
    session_.draw(Vector2{frameWidth, frameHeight}, dp(), now_, seconds);
    if (volumeOsd_.visible(now_)) {
        paintVolumeOsd(volumeOsd_.level(), Vector2{frameWidth, frameHeight}, dp(), hud_.topInset(),
                       volumeOsd_.look(now_));
    }
    hud_.drawToast();
    EndBlendMode();
    if (target != nullptr) {
        EndTextureMode();
    }
    EndDrawing();
}

} // namespace opensu::ui

#include "shell.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include "input/keyboard_bindings.hpp"
#include "lucent/log.h"
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
    if (std::holds_alternative<library::Giveaways>(item)) {
        return TileKind::Giveaways;
    }
    if (std::holds_alternative<library::AddGame>(item)) {
        return TileKind::AddGame;
    }
    return std::holds_alternative<library::AllGames>(item) ? TileKind::AllGames : TileKind::Game;
}

ScrollMode scrollModeFor(config::HomeMode mode) noexcept {
    return mode == config::HomeMode::WiiSu ? ScrollMode::Paged : ScrollMode::Flow;
}

/// What a tile stands for across a catalogue update: a game by its id, a folder by its key.
std::string shelfKey(const library::ShelfItem& item) {
    if (const auto* game = std::get_if<library::Game>(&item)) {
        return game->id;
    }
    if (std::holds_alternative<library::Giveaways>(item)) {
        return "giveaways";
    }
    if (std::holds_alternative<library::AddGame>(item)) {
        return "add-game";
    }
    const std::optional<library::Folder> folder = library::folderOf(item);
    return folder ? "folder:" + library::key(*folder) : std::string{};
}

std::pair<std::filesystem::path, std::filesystem::path> artPaths(const library::ShelfItem& item) {
    if (const auto* console = std::get_if<library::Console>(&item)) {
        return {console->artwork, {}};
    }
    if (const auto* game = std::get_if<library::Game>(&item)) {
        return {game->artwork, game->artworkWide};
    }
    return {{}, {}};
}

bool intersectsCanvas(const Rect& rect, int width, int height) noexcept {
    // Scaled tiles grow past their cell, so the test is one tile wide on each side.
    return rect.right() + rect.width > 0.0f && rect.x - rect.width < static_cast<float>(width) &&
           rect.bottom() + rect.height > 0.0f && rect.y - rect.height < static_cast<float>(height);
}

} // namespace

Shell::Shell(int width, int height, config::HomeMode mode, const std::filesystem::path& assetsDir)
    : text_{assetsDir}, icons_{assetsDir}, layout_{HomeLayoutInput{}}, mode_{mode},
      width_{width}, height_{height} {
    hud_.setSize({width, height, dp()});
    relayout();
}

Shell::~Shell() {
    artwork_.beginShelf(tiles_, header_);
}

void Shell::attach(SDL_GPUDevice& device) {
    text_.load(device);
    icons_.attach(device);
    device_ = &device;
}

SDL_GPUDevice& Shell::device() const {
    if (device_ == nullptr) {
        throw std::logic_error{"the shell drew before it was attached to a device"};
    }
    return *device_;
}

bool Shell::chromeDark() noexcept {
    // iiSU gh3.q: the grid chrome is dark when the theme background's luminance is under half.
    return luminance(palette::ground) < 0.5f;
}

float Shell::aboveKeyboard() const {
    const auto height = static_cast<float>(height_);
    if (!keyboard_.isOpen()) {
        return height;
    }
    return layoutKeyboard(Rect{0.0f, 0.0f, static_cast<float>(width_), height}, dp(),
                          keyboard_.style())
        .panel.y;
}

float Shell::dp() const noexcept {
    return std::min(static_cast<float>(width_) / referenceWidthDp,
                    static_cast<float>(height_) / referenceHeightDp) *
           uiScale_;
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
        .iconLevel = iconSize_,
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
    fillTile(tile, std::move(item));
    return tile;
}

void Shell::fillTile(Tile& tile, library::ShelfItem item) const {
    tile.item = std::move(item);
    tile.platform = platforms_.forItem(tile.item);
    tile.title.clear();
    tile.caption.clear();
    tile.logo.reset();
    tile.stores.clear();
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
    } else if (std::holds_alternative<library::Giveaways>(tile.item)) {
        tile.title = std::string{library::giveawaysName};
        tile.caption = "Free to keep";
    } else if (std::holds_alternative<library::AddGame>(tile.item)) {
        tile.title = std::string{library::addGameName};
        tile.caption = "A file, Steam shortcuts, Lutris";
    } else {
        for (const library::Source source : std::get<library::Game>(tile.item).ownedIn) {
            if (const std::optional<Icon> icon = iconFor(source)) {
                tile.stores.push_back(*icon);
            }
        }
    }
}

void Shell::setHeader(std::optional<library::ShelfItem> header) {
    if (header_) {
        artwork_.unload(*header_);
    }
    header_.reset();
    if (header) {
        header_ = makeTile(std::move(*header));
        header_->id = nextTileId_++;
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
        tiles_.back().id = nextTileId_++;
        tiles_.back().ticket = artwork_.ticket();
    }
    relayout();
    const std::size_t start = focus < tiles_.size() ? focus : 0;
    focus_.reset(start, layout_.cellOf(start));
    page_ = layout_.mode() == ScrollMode::Paged ? layout_.pageOf(start) : 0;
    scroller_.snap(layout_.scrollTarget({start, 0.0f, 0}));
    railFocus_.snap(static_cast<float>(start));
    focusAt_ = now_;
    entrancePending_ = true;
}

void Shell::updateShelf(std::vector<library::ShelfItem> items) {
    const bool first = tiles_.empty() && !items.empty();
    const std::string focused =
        focus_.index() < tiles_.size() ? shelfKey(tiles_[focus_.index()].item) : std::string{};
    std::unordered_map<std::string, std::size_t> before;
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        before.emplace(shelfKey(tiles_[i].item), i);
    }
    std::vector<Tile> next;
    next.reserve(items.size());
    std::vector<bool> carried(tiles_.size(), false);
    for (library::ShelfItem& item : items) {
        const std::string key = shelfKey(item);
        const auto found = before.find(key);
        if (found != before.end() && !carried[found->second]) {
            carried[found->second] = true;
            Tile& tile = tiles_[found->second];
            const auto art = artPaths(tile.item);
            fillTile(tile, std::move(item));
            if (artPaths(tile.item) != art) {
                artwork_.unload(tile);
            }
            next.push_back(std::move(tile));
        } else {
            next.push_back(makeTile(std::move(item)));
            next.back().id = nextTileId_++;
            next.back().ticket = artwork_.ticket();
        }
    }
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        if (!carried[i]) {
            artwork_.unload(tiles_[i]);
        }
    }
    tiles_ = std::move(next);
    gameTiles_.clear();
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        if (const auto* game = std::get_if<library::Game>(&tiles_[i].item)) {
            gameTiles_.emplace(game->id, i);
        }
    }
    relayout();
    std::size_t start = 0;
    if (!focused.empty()) {
        const auto at = std::ranges::find_if(tiles_, [&focused](const Tile& tile) {
            return shelfKey(tile.item) == focused;
        });
        if (at != tiles_.end()) {
            start = static_cast<std::size_t>(at - tiles_.begin());
        }
    }
    if (start >= tiles_.size()) {
        start = tiles_.empty() ? 0 : tiles_.size() - 1;
    }
    focus_.reset(start, layout_.cellOf(start));
    page_ = layout_.mode() == ScrollMode::Paged ? layout_.pageOf(start) : 0;
    scroller_.snap(layout_.scrollTarget({start, 0.0f, 0}));
    railFocus_.snap(static_cast<float>(start));
    focusAt_ = now_;
    entrancePending_ = first;
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
    SDL_GPUDevice& gpu = device();
    glyphs_.load(gpu);
    // Covers are point-sampled.
    artwork_.load(
        [&gpu](const gpu2d::image::Bitmap& pixels) {
            return gpu2d::Texture{gpu, pixels.rgba, pixels.width, pixels.height,
                                  gpu2d::Filter::Nearest};
        },
        tiles_, header_, artWindow(), uploads);
    const auto linear = [&gpu](const gpu2d::image::Bitmap& pixels) {
        return gpu2d::Texture{gpu, pixels.rgba, pixels.width, pixels.height, gpu2d::Filter::Linear};
    };
    screens_.images().load(linear);
}

void Shell::loadArtworkNow() {
    loadArtwork(std::numeric_limits<std::size_t>::max());
    while (!artwork_.idle()) {
        artwork_.waitDecoded();
        loadArtwork(std::numeric_limits<std::size_t>::max());
    }
}

std::size_t Shell::artworkPending() const {
    return artwork_.pending(tiles_, header_, artWindow());
}

std::size_t Shell::loadedArtwork() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(tiles_, [](const Tile& tile) {
        return tile.portrait.has_value();
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

void Shell::setUiScale(float factor) {
    if (factor != uiScale_) {
        uiScale_ = factor;
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
    followFade(keyboardFade_, keyboard_.isOpen(), keyboardWasOpen_);
    followFade(mappingFade_, mapping_.isOpen(), mappingWasOpen_);
    volumeOsd_.tick(now_);
    railFocus_.step(dt);
    if (layout_.mode() == ScrollMode::Flow) {
        scroller_.step(dt, layout_.cellWidth() + layout_.gap(), layout_.viewportWidth(),
                       ScrollMode::Flow);
    }
    hud_.tick(now);
    // Overlays stand above the docked keyboard, so the field it types into shows.
    const auto height = static_cast<float>(height_);
    screens_.layout(now_, gpu2d::Rect{0.0f, 0.0f, static_cast<float>(width_), height},
                    height - aboveKeyboard(), dp(), Chrome{hud_.topInset()});
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
    if (focusedGiveaways()) {
        return std::string{library::giveawaysName};
    }
    if (focusedAddGame()) {
        return std::string{library::addGameName};
    }
    return {};
}

bool Shell::focusedGiveaways() const {
    return focus_.index() < tiles_.size() &&
           std::holds_alternative<library::Giveaways>(tiles_[focus_.index()].item);
}

bool Shell::focusedAddGame() const {
    return focus_.index() < tiles_.size() &&
           std::holds_alternative<library::AddGame>(tiles_[focus_.index()].item);
}

std::string Shell::pillTitle() const {
    // The grid names its focused tile; a page that replaces the grid names itself in the trail.
    const bool page = screens_.active();
    return presentation() == Presentation::Grid && !page ? focusedTitle() : std::string{};
}

void Shell::fillContent(TileVisual& visual, const Tile& tile) const {
    visual.art = tile.art();
    visual.platform = tile.platform;
    visual.loading = tile.awaitingArt();
    if (const auto* game = std::get_if<library::Game>(&tile.item)) {
        visual.title = game->title;
        visual.stores = tile.stores;
        visual.update = game->updateAvailable;
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

void Shell::drawGrid(Canvas& canvas) {
    // iiSU nx2.d: unfocused tiles first, the focused tile last, over its neighbours.
    const SlotRange seen = layout_.visibleSlots(scroll(), static_cast<float>(width_));
    const std::size_t focused = focus_.index();
    for (std::size_t slot = seen.first; slot < seen.last; ++slot) {
        if (slot == focused) {
            continue;
        }
        const TileVisual visual = visualFor(slot, layout_.canvasRect(slot, scroll()));
        if (intersectsCanvas(visual.rect, width_, height_)) {
            tilePainter_.paint(canvas, visual);
        }
    }
    const TileVisual visual = visualFor(focused, layout_.canvasRect(focused, scroll()));
    if (intersectsCanvas(visual.rect, width_, height_)) {
        tilePainter_.paint(canvas, visual);
    }
}

float Shell::aspectOf(const Tile& tile) const noexcept {
    const gpu2d::Texture* art = tile.art();
    return art != nullptr && art->height() > 0
               ? static_cast<float>(art->width()) / static_cast<float>(art->height())
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

void Shell::drawRail(Canvas& canvas) {
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
            tilePainter_.paint(canvas, visualFor(first + slot, rects[slot]));
        }
    }
    if (tiles_.empty()) {
        return;
    }
    const std::size_t focused = std::min(focus_.index(), tiles_.size() - 1);
    const std::string title = focusedTitle();
    const RailStyle style{chromeDark(), railAlpha(1)};
    if (!xmb) {
        railPainter_.paintTitle(canvas, row, title, style);
        if (header_) {
            railPainter_.paintRowMarker(canvas, row, style);
        }
        return;
    }
    railPainter_.paintTitle(canvas, column, title, style);
    const RailStyle columnStyle{style.dark, railAlpha(0)};
    if (header_) {
        TileVisual card;
        card.rect = outerForContent(column.headerCard());
        card.cell = column.headerCard().width;
        card.dark = style.dark;
        card.selectionRing = false;
        card.alpha = columnStyle.alpha;
        fillContent(card, *header_);
        tilePainter_.paint(canvas, card);
        railPainter_.paintColumnMarker(canvas, column, columnStyle);
        return;
    }
    Focused source;
    source.platform = tiles_[focused].platform;
    if (const auto* console = std::get_if<library::Console>(&tiles_[focused].item)) {
        source.key = console->system;
        source.card = console->artwork;
    }
    railPainter_.paintColumn(canvas, column, source, columnStyle);
}

gpu2d::RenderTarget& Shell::sceneTarget() {
    if (!scene_ || scene_->width() != width_ || scene_->height() != height_) {
        scene_.emplace(device(), width_, height_);
    }
    return *scene_;
}

Shell::DockFrame Shell::dockFrame(float width, float height) const {
    const float pixelsPerDp = dp();
    const std::vector<float> widths{plainItem, wideItem};
    const DockMetrics metrics{width / pixelsPerDp, height / pixelsPerDp, widths};
    return DockFrame{metrics, layoutDock(metrics, width, height, pixelsPerDp),
                     DockStyle{chromeDark(), pixelsPerDp, dockVisibility_.progress(nowMs())}};
}

PointerTarget Shell::pointAt(std::optional<Point> point) {
    dockHover_.reset();
    hud_.setLauncherHover(std::nullopt);
    hud_.setCrumbHover(std::nullopt);
    if (!point) {
        return {};
    }
    if (!inGame_ && (!panelOpen() || (screens_.active() && !keyboard_.isOpen())) &&
        !mapping_.isOpen()) {
        if (const std::optional<std::size_t> crumb = hud_.crumbAt(point->x, point->y)) {
            hud_.setCrumbHover(crumb);
            return OnCrumb{*crumb};
        }
    }
    if (inGame_ || panelOpen()) {
        return pointAtModal(*point);
    }
    if (const LauncherBadge* badge = hud_.launcherAt(point->x, point->y)) {
        hud_.setLauncherHover(badge->source);
        return OnLauncher{badge->source, badge->progress.has_value()};
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

PointerTarget Shell::pointAtModal(Point point) const {
    // The mapping editor takes the pad only.
    if (mapping_.isOpen() || (inGame_ && !screens_.active())) {
        return {};
    }
    if (keyboard_.isOpen() && point.y >= aboveKeyboard()) {
        const KeyboardLayout keys = layoutKeyboard(
            Rect{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)}, dp(),
            keyboard_.style());
        if (const std::optional<std::size_t> key = keys.keyAt(point.x, point.y)) {
            return OnKeyboardKey{*key};
        }
        return {};
    }
    if (screens_.active()) {
        return OnScreens{};
    }
    return {};
}

PointerTarget Shell::pointAtHome(Point point) const {
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
            if constexpr (std::is_same_v<Target, OnKeyboardKey>) {
                return keyboard_.isOpen() && keyboard_.focusKey(at.index);
            } else if constexpr (std::is_same_v<Target, OnTile>) {
                return focusTile(at.index);
            } else if constexpr (std::is_same_v<Target, OnPage>) {
                return focusPage(at.page);
            } else {
                return false;
            }
        },
        target);
}

void Shell::drawDock(Canvas& canvas, const gpu2d::RenderTarget& scene, const DockFrame& frame) {
    const double now = nowMs();
    const Rect bar = DockPainter::barRect(frame.layout, frame.style);
    canvas.blurBehind(gpu2d::Backdrop{.scene = &scene,
                                      .body = onCanvas(bar),
                                      .radius = bar.height * 0.5f,
                                      .sigma = glassBlurSigmaDp * dp(),
                                      .opacity = std::clamp(frame.style.progress, 0.0f, 1.0f)});
    std::array<DockIcon, library::allSections.size()> icons;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const library::Section section = library::allSections[i];
        icons[i] = DockIcon{.section = section,
                            .selected = section == section_,
                            .scale = iconPops_[i].scale(now),
                            .hovered = dockHover_ == section};
    }
    dockPainter_.paint(canvas, frame.layout, frame.metrics, icons, frame.style);
}

void Shell::drawScene(Canvas& canvas) {
    hud_.drawGround(canvas);
    const bool pageShown = screens_.pagesVisible();
    if (presentation() == Presentation::Grid) {
        drawGrid(canvas);
        if (!pageShown) {
            pillPainter_.paint(canvas, layout_.pagePill(page_), dp(), chromeDark());
            arrowPainter_.paint(canvas, layout_.pageArrows(page_), chromeDark());
        }
    } else {
        drawRail(canvas);
    }
    screens_.paintPages(canvas);
    hud_.drawTopBar(canvas);
    hud_.drawHints(canvas);
}

void Shell::drawOverGame(Canvas& canvas, Point size) {
    // Only the Guide menu and what it opens are drawn; the game shows through everywhere else.
    screens_.paintPages(canvas);
    // A panel over the game (a game's settings) leaves the top bar out; a page has it.
    if (screens_.paged()) {
        hud_.drawTopBar(canvas);
        hud_.drawHints(canvas);
    }
    mappingPainter_.paint(canvas, mapping_, size, dp(), mappingFade_.look(now_));
    screens_.paintOverlays(canvas);
    keyboardPainter_.paint(canvas, keyboard_, size, dp(), keyboardFade_.look(now_));
    drawChordHint(canvas, size);
    hud_.drawToast(canvas, aboveKeyboard());
}

void Shell::drawChordHint(Canvas& canvas, Point size) {
    if (chordHint_.visible(now_)) {
        chordHintPainter_.paint(canvas, chordHint_, size, dp(), chordHint_.look(now_));
    }
}

void Shell::draw(Canvas& canvas, const gpu2d::Surface& target) {
    // The previous frame was submitted, so the labels it did not draw can go.
    text_.retire();
    const Point size{static_cast<float>(target.width), static_cast<float>(target.height)};
    // Transparent, so a compositor shows what lies below wherever nothing is drawn.
    constexpr Colour clear{0, 0, 0, 0};
    if (inGame_) {
        canvas.begin(target, clear);
        drawOverGame(canvas, size);
        return;
    }

    // The scene goes to its own target so the dock's glass can blur what is behind it.
    const gpu2d::RenderTarget& scene = sceneTarget();
    canvas.begin(scene.surface(), clear);
    drawScene(canvas);

    canvas.begin(target, clear);
    const gpu2d::Rect whole{0.0f, 0.0f, static_cast<float>(scene.width()),
                            static_cast<float>(scene.height())};
    canvas.image(scene.view(gpu2d::Filter::Nearest), whole, whole, white);
    float toastFloor = aboveKeyboard();
    if (!keyboard_.isOpen() && !screens_.pagesVisible()) {
        const DockFrame dock = dockFrame(size.x, size.y);
        drawDock(canvas, scene, dock);
        toastFloor = std::min(toastFloor, DockPainter::top(dock.layout, dock.style));
    }
    mappingPainter_.paint(canvas, mapping_, size, dp(), mappingFade_.look(now_));
    screens_.paintOverlays(canvas);
    keyboardPainter_.paint(canvas, keyboard_, size, dp(), keyboardFade_.look(now_));
    if (volumeOsd_.visible(now_)) {
        volumeOsdPainter_.paint(canvas, volumeOsd_.level(), size, dp(), hud_.topInset(),
                                volumeOsd_.look(now_));
    }
    drawChordHint(canvas, size);
    hud_.drawToast(canvas, toastFloor);
}

} // namespace opensu::ui

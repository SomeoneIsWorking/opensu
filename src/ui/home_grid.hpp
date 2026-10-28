// home_grid — the home screen's tiles: the grid (Flow or Paged) or Library's XMB or Carousel rail.
//
// Composes the grid's owners: layout (where cells sit, or a rail's slots), focus (which tile a
// press reaches), motion (time-based scale and scroll), and the tile, rail, pill and arrow
// painters. It is the cells of the home screen's `gpu2d::ui::CellGrid`, which moves, points at
// and draws them through it.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/ui/cell_grid.hpp"

#include "config/config.hpp"
#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "interface_text.hpp"
#include "library/game.hpp"
#include "library/sections.hpp"
#include "library/shelf.hpp"
#include "page_arrow.hpp"
#include "page_pill.hpp"
#include "paint.hpp"
#include "platform.hpp"
#include "rail_layout.hpp"
#include "rail_painter.hpp"
#include "section_view.hpp"
#include "tile.hpp"
#include "tile_artwork.hpp"
#include "tile_motion.hpp"
#include "tile_painter.hpp"
#include "vector_icon.hpp"

namespace opensu::ui {

class HomeGrid final : public gpu2d::ui::CellGrid::Cells {
  public:
    using Clock = std::chrono::steady_clock;

    /// The window the grid fills, its pixels per dp and the room the chrome keeps at its edges.
    struct Frame {
        int width{};
        int height{};
        float dp{1.0f};
        float topInset{};
        float bottomInset{};
    };

    HomeGrid(config::HomeMode mode, InterfaceText& text, IconAtlas& icons);
    ~HomeGrid() override;
    HomeGrid(const HomeGrid&) = delete;
    HomeGrid& operator=(const HomeGrid&) = delete;
    HomeGrid(HomeGrid&&) = delete;
    HomeGrid& operator=(HomeGrid&&) = delete;

    /// Re-flows the grid in `frame`.
    void setFrame(const Frame& frame);

    /// Replaces the grid's entries, releasing their artwork, focuses `focus` and plays the
    /// entrance.
    void setShelf(std::vector<library::ShelfItem> items, std::size_t focus = 0);
    /// Replaces the entries in place: the focused tile keeps the focus, or its slot when it went.
    void updateShelf(std::vector<library::ShelfItem> items);

    /// Sets the card the XMB's left column shows inside a folder, standing for the folder, or
    /// clears it (navigation.md §5.4).
    void setHeader(std::optional<library::ShelfItem> header);

    /// Streams the artwork of the tiles near the canvas: queues their files to decode on the
    /// decoder's thread and uploads at most `uploads` decoded images as textures on `device`, then
    /// releases the textures of tiles far from the canvas once many are held. Once a frame.
    void loadArtwork(SDL_GPUDevice& device, std::size_t uploads);
    /// Loads the artwork of the tiles near the canvas and waits until it is all in; for a still
    /// render.
    void loadArtworkNow(SDL_GPUDevice& device);

    /// Gives a game's tile artwork that arrived after the shelf was set.
    void setArtwork(std::string_view gameId, const std::filesystem::path& artwork);
    /// Sets which games' artwork is downloading or queued, which is what a tile's spinner shows;
    /// every other game's is not.
    void setArtworkDownloading(const std::vector<std::string>& gameIds);
    /// The games near the canvas whose artwork is downloading or queued, for the fetcher to do
    /// first.
    [[nodiscard]] std::vector<std::string> downloadingOnScreen() const;
    /// A game's artwork is not coming (a confirmed miss, or the round ended): no spinner.
    void artworkSettled(std::string_view gameId);

    /// Lays out `section`'s shelf: Home fills its pages with empty slots, Library does not.
    void setSection(library::Section section);
    /// Sets how Library lays out its tiles; takes effect in place.
    void setLibraryMode(library::LibraryMode mode);
    [[nodiscard]] library::LibraryMode libraryMode() const noexcept {
        return libraryMode_;
    }
    [[nodiscard]] Presentation presentation() const noexcept {
        return presentationOf(section_, libraryMode_);
    }
    /// Sets iiSU's icon size level (1 to 20) Library's tiles are laid out at; the grid, the XMB
    /// and the Carousel all follow it.
    void setIconSize(int level);
    [[nodiscard]] int iconSize() const noexcept {
        return iconSize_;
    }
    /// Sets how the home grid scrolls; takes effect in place.
    void setHomeMode(config::HomeMode mode);
    /// Whether the page pill and arrows show: not while a page covers the grid.
    void showPageControls(bool shown) noexcept {
        pageControls_ = shown;
    }

    /// Focuses slot `index` of the grid or tile `index` of the rail; reports whether it moved.
    bool focusTile(std::size_t index);
    /// Starts the press pulse on the focused tile.
    void pressFocused();

    /// Advances motion to `now`.
    void tick(Clock::time_point now);
    /// Advances past every one-shot motion, for a still frame.
    void settle();

    // CellGrid::Cells: the D-pad, the pointer and the wheel, and drawing.
    bool move(gpu2d::ui::Direction direction) override;
    bool focus(std::size_t index) override;
    bool turn(std::size_t page) override;
    bool step(int steps) override;
    [[nodiscard]] std::optional<gpu2d::ui::CellGrid::Spot> at(Point at) const override;
    void paint(gpu2d::ui::Painter& painter, const gpu2d::Rect& frame) override;

    [[nodiscard]] const std::vector<Tile>& tiles() const noexcept {
        return tiles_;
    }
    /// How many tiles ended up with a loaded portrait texture.
    [[nodiscard]] std::size_t loadedArtwork() const noexcept;
    /// Tiles near the canvas still showing the loading placeholder.
    [[nodiscard]] std::size_t artworkPending() const;
    [[nodiscard]] int pageCount() const noexcept {
        return layout_.pageCount();
    }
    [[nodiscard]] int page() const noexcept {
        return page_;
    }
    /// The focused game, or null when a console or an empty slot has focus.
    [[nodiscard]] const library::Game* focusedGame() const;
    /// The focused tile's entry, or null for an empty slot.
    [[nodiscard]] const library::ShelfItem* focusedItem() const;
    /// The folder the focused tile opens, or nothing for a game or an empty slot.
    [[nodiscard]] std::optional<library::Folder> focusedFolder() const;
    /// Whether the focused tile is Giveaways.
    [[nodiscard]] bool focusedGiveaways() const;
    /// Whether the focused tile is Add a game.
    [[nodiscard]] bool focusedAddGame() const;
    /// The title the focused entry shows, empty for an empty slot.
    [[nodiscard]] std::string focusedTitle() const;
    /// The title for the top bar's pill: the grid's, where an XMB and a Carousel draw their own.
    [[nodiscard]] std::string pillTitle() const;
    [[nodiscard]] std::size_t focusIndex() const noexcept {
        return focus_.index();
    }
    [[nodiscard]] const HomeLayout& layout() const noexcept {
        return layout_;
    }

  private:
    [[nodiscard]] HomeLayout computeLayout() const;
    [[nodiscard]] float sinceMs(Clock::time_point then) const noexcept;
    [[nodiscard]] double nowMs() const noexcept;
    [[nodiscard]] float scroll() const noexcept;
    void relayout();
    /// Moves an XMB's or a Carousel's focus one tile along its axis; Up and Down for the column,
    /// Left and Right for the row (input-sound.md §1.4).
    bool moveRail(gpu2d::ui::Direction direction);
    /// Turns a Paged grid to `page`, focusing the same row's first or last slot there; reports
    /// whether it turned.
    bool focusPage(int page);
    [[nodiscard]] float aspectOf(const Tile& tile) const noexcept;
    /// What an XMB or a Carousel is laid out from, at the eased focus.
    [[nodiscard]] RailInput railInput() const;
    /// The rectangle a tile is given in a slot of the XMB or the Carousel, so that its frame's edge
    /// is the slot's: a console's card sits 2 dp inside it, a game's fills it (navigation.md
    /// §5.3).
    [[nodiscard]] Rect railSlot(const Tile& tile, const Rect& rect) const noexcept;
    /// The rectangles an XMB's or a Carousel's layout `rects` are drawn at, tile by tile, for the
    /// tiles from `first` on.
    [[nodiscard]] std::vector<Rect> railSlots(const std::vector<Rect>& rects,
                                              std::size_t first) const;
    /// How far the entrance has run for a tile `distance` from the focus, or 1 when none runs.
    [[nodiscard]] float railAlpha(int distance) const noexcept;
    [[nodiscard]] Tile makeTile(library::ShelfItem item) const;
    /// What `item` shows, leaving `tile`'s artwork and id.
    void fillTile(Tile& tile, library::ShelfItem item) const;
    /// The tiles whose art is wanted now: those near the canvas, as the layout owner places them.
    [[nodiscard]] SlotRange artWindow() const;
    /// Fills `visual` with what `tile` shows: its art, frame, names and badges.
    void fillContent(TileVisual& visual, const Tile& tile) const;
    [[nodiscard]] TileVisual visualFor(std::size_t slot, const Rect& rect) const;
    /// Points focus at `index`, restarting the focus scale and bringing it into view.
    void focusOn(std::size_t index, int dx);
    void startEntrance();
    void drawGrid(Canvas& canvas);
    void drawRail(Canvas& canvas);

    std::vector<Tile> tiles_;
    /// A game's tile in `tiles_`, by game id.
    std::unordered_map<std::string, std::size_t> gameTiles_;
    /// The id the next tile takes.
    std::uint64_t nextTileId_{1};
    TileArtwork artwork_;
    /// What stands for the folder in the XMB's left column.
    std::optional<Tile> header_;
    Platforms platforms_;
    HomeLayout layout_;
    GridFocus focus_;
    motion::ScrollEaser scroller_;
    /// Where an XMB's or a Carousel's focus has eased to, in tiles.
    motion::VisualIndexEaser railFocus_;
    TilePainter tilePainter_;
    RailPainter railPainter_;
    PagePillPainter pillPainter_;
    PageArrowPainter arrowPainter_;

    Clock::time_point now_{Clock::now()};
    Clock::time_point lastTick_{now_};
    Clock::time_point focusAt_{now_};
    std::optional<Clock::time_point> pressAt_;
    std::size_t pressIndex_{};
    std::optional<Clock::time_point> entranceAt_;

    config::HomeMode mode_;
    Frame frame_;
    library::Section section_{library::Section::Home};
    library::LibraryMode libraryMode_{library::LibraryMode::Standard};
    int page_{};
    int entranceFirstStep_{};
    int entranceSpread_{};
    int iconSize_{defaultIconLevel};
    bool entrancePending_{false};
    bool pageControls_{true};
};

} // namespace opensu::ui

// ui — the drawn home screen.
//
// Composes the home grid's owners: layout (where cells sit, or an XMB's or a Carousel's rail),
// focus (which tile a press reaches), motion (time-based scale and scroll), the tile and pill
// painters, the dock and the HUD around them.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "raylib.h"

#include "backdrop_blur.hpp"
#include "config/config.hpp"
#include "dock_metrics.hpp"
#include "dock_motion.hpp"
#include "dock_painter.hpp"
#include "game_menu.hpp"
#include "game_menu_painter.hpp"
#include "glyph_textures.hpp"
#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "hud.hpp"
#include "image_decoder.hpp"
#include "input/last_device.hpp"
#include "launch_panel.hpp"
#include "launch_panel_painter.hpp"
#include "library/game.hpp"
#include "library/sections.hpp"
#include "library/shelf.hpp"
#include "mode_chooser.hpp"
#include "mode_chooser_painter.hpp"
#include "page_arrow.hpp"
#include "page_pill.hpp"
#include "platform.hpp"
#include "pointer_target.hpp"
#include "rail_layout.hpp"
#include "rail_painter.hpp"
#include "section_view.hpp"
#include "tile_motion.hpp"
#include "tile_painter.hpp"

namespace opensu::ui {

/// One entry in the grid, with the artwork loaded for it.
struct Tile {
    library::ShelfItem item;
    /// A folder's name, as its tile reads it.
    std::string title;
    /// A folder's game count, as its tile reads it.
    std::string caption;
    /// A launcher's logo.
    std::optional<Icon> logo;
    /// The stores a game is owned in, as icons in its corner.
    std::vector<Icon> stores;
    /// Artwork loaded as textures, or zero for none.
    Texture portrait{};
    Texture wide{};
    bool hasPortrait{false};
    bool hasWide{false};
    /// Whether its artwork files have been read into the textures.
    bool artLoaded{false};
    /// Whether its files are queued to decode or decoding.
    bool artRequested{false};
    /// Whether the artwork fetcher is downloading, or has queued, art for it.
    bool artDownloading{false};
    /// Changes when its files change, so a decode of the old ones is dropped.
    std::uint64_t ticket{};
    /// The tile's platform frame, or null for a game with no platform identity.
    const Platform* platform{nullptr};
};

/// The home screen.
class Shell {
  public:
    using Clock = std::chrono::steady_clock;

    Shell(int width, int height, config::HomeMode mode);
    ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    /// Reports a new window size and re-flows the grid.
    void setSize(int width, int height);

    /// Replaces the grid's entries, releasing the artwork already loaded, focuses `focus` and
    /// plays the entrance.
    void setShelf(std::vector<library::ShelfItem> items, std::size_t focus = 0);

    /// Sets the card the XMB's left column shows inside a folder, standing for the folder, or
    /// clears it (navigation.md §5.4).
    void setHeader(std::optional<library::ShelfItem> header);

    /// Streams the artwork of the tiles near the canvas: queues their files to decode on the
    /// decoder's thread and uploads at most `uploads` decoded images as textures, then releases
    /// the textures of tiles far from the canvas once more than `residentLimit` are held. Drawing
    /// does this itself; it needs the GL context.
    void loadArtwork(std::size_t uploads = uploadsPerFrame);

    /// Loads the artwork of the tiles near the canvas and waits until it is all in; for a still
    /// render.
    void loadArtworkNow();

    /// Textures uploaded per frame, so a screenful of new covers spreads over a few frames.
    static constexpr std::size_t uploadsPerFrame = 4;
    /// Tile textures held at most; the oldest outside the window are released beyond it.
    static constexpr std::size_t residentLimit = 120;

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
    /// Gives a console's tile the card that arrived after the shelf was set.
    void setConsoleArtwork(std::string_view system, const std::filesystem::path& artwork);

    /// Gives the ROM tiles of a system the glyph for their frame's tab.
    void setGlyph(std::string_view system, const std::filesystem::path& glyph);

    /// Resolves an entry's platform frame. A title from a store has no console, so
    /// it is framed in that store's identity; a ROM and a console in their system's.
    [[nodiscard]] const Platform* platformFor(const library::ShelfItem& item) const;

    /// Makes `section` the one shown, which sets its grid and where the dock stands. The shelf is
    /// the caller's to replace after. `revealDock` slides the dock in for a moment (iiSU
    /// `jk2.java:937`), as an L1 or R1 press does.
    void setSection(library::Section section, bool revealDock);
    [[nodiscard]] library::Section section() const noexcept {
        return section_;
    }

    /// Sets how Library lays out its tiles; takes effect in place.
    void setLibraryMode(library::LibraryMode mode);
    [[nodiscard]] library::LibraryMode libraryMode() const noexcept {
        return libraryMode_;
    }
    [[nodiscard]] Presentation presentation() const noexcept {
        return presentationOf(section_, libraryMode_);
    }

    /// Which device the prompts name. The caller feeds it; the painters read it.
    [[nodiscard]] input::LastDevice& inputDevice() noexcept {
        return device_;
    }

    /// The pointer at `point`, or away from the window with nothing. Hovers the dock item under
    /// it and slides a hidden dock in when the pointer rests where the dock is. Returns what the
    /// pointer is on.
    PointerTarget pointAt(std::optional<Vector2> point);

    /// Moves the focus the D-pad moves to the element in `target`: a tile, a page, a layout card
    /// or a menu row. Reports whether it moved.
    bool focusTarget(const PointerTarget& target);
    /// Focuses slot `index` of the grid or tile `index` of the rail; reports whether it moved.
    bool focusTile(std::size_t index);
    /// Turns a Paged grid to `page`, focusing the same row's first or last slot there; reports
    /// whether it turned.
    bool focusPage(int page);

    /// Gives a dock icon's file, which arrives from the APK after the shell is up.
    void setNavIcon(library::Section section, bool selected, const std::filesystem::path& file);

    /// The picker for Library's layout, and whether it is up.
    [[nodiscard]] ModeChooser& modeChooser() noexcept {
        return chooser_;
    }
    [[nodiscard]] const ModeChooser& modeChooser() const noexcept {
        return chooser_;
    }

    /// Moves focus, and reports whether it moved.
    bool moveFocus(Direction direction);

    /// Focuses the first tile.
    void resetFocus();

    /// Starts the press pulse on the focused tile.
    void pressFocused();

    /// Advances motion and the toast to `now`.
    void tick(Clock::time_point now);

    /// Advances past every one-shot motion, for a still frame.
    void settle();

    /// Draws one frame: the home screen, or while a game runs, only the Guide menu over a
    /// transparent frame. With `target`, a render texture, the frame goes there instead of the
    /// window (the window's buffers are still begun and ended, as a frame is).
    void draw(const RenderTexture2D* target = nullptr);

    /// Whether a game is running, which turns the home screen into the in-game overlay.
    void setInGame(bool inGame) noexcept {
        inGame_ = inGame;
    }
    [[nodiscard]] bool inGame() const noexcept {
        return inGame_;
    }
    [[nodiscard]] GameMenu& gameMenu() noexcept {
        return gameMenu_;
    }
    [[nodiscard]] const GameMenu& gameMenu() const noexcept {
        return gameMenu_;
    }
    [[nodiscard]] LaunchPanel& launchPanel() noexcept {
        return launchPanel_;
    }

    [[nodiscard]] const std::vector<Tile>& tiles() const noexcept {
        return tiles_;
    }
    /// How many tiles ended up with a loaded portrait texture.
    [[nodiscard]] std::size_t loadedArtwork() const noexcept;
    [[nodiscard]] int pageCount() const noexcept {
        return layout_.pageCount();
    }
    [[nodiscard]] int page() const noexcept {
        return page_;
    }
    /// The focused game, or null when a console or an empty slot has focus.
    [[nodiscard]] const library::Game* focusedGame() const;
    /// The folder the focused tile opens, or nothing for a game or an empty slot.
    [[nodiscard]] std::optional<library::Folder> focusedFolder() const;
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
    [[nodiscard]] const std::string& status() const noexcept {
        return hud_.status();
    }
    [[nodiscard]] const std::string& toast() const noexcept {
        return hud_.toast();
    }
    [[nodiscard]] bool toastIsError() const noexcept {
        return hud_.toastIsError();
    }

    void setStatus(std::string text) {
        hud_.setStatus(std::move(text));
    }
    void setClock(std::string text) {
        hud_.setClock(std::move(text));
    }
    void setToast(std::string text, bool isError = false) {
        hud_.setToast(std::move(text), isError, now_);
    }
    void setLaunchers(std::vector<LauncherBadge> launchers) {
        hud_.setLaunchers(std::move(launchers));
    }
    void setTitle(std::string title) {
        hud_.setTitle(std::move(title));
    }
    void setBattery(std::optional<device::BatteryStatus> battery) noexcept {
        hud_.setBattery(battery);
    }

  private:
    /// Pixels per dp.
    [[nodiscard]] float dp() const noexcept;
    /// Dark or light grid chrome for the current theme (iiSU ya0.e).
    [[nodiscard]] static bool chromeDark() noexcept;
    [[nodiscard]] HomeLayout computeLayout() const;
    [[nodiscard]] float sinceMs(Clock::time_point then) const noexcept;
    [[nodiscard]] float scroll() const noexcept;

    void relayout();
    /// Moves an XMB's or a Carousel's focus one tile along its axis; Up and Down for the column,
    /// Left and Right for the row (input-sound.md §1.4).
    bool moveRail(Direction direction);
    [[nodiscard]] double nowMs() const noexcept;
    [[nodiscard]] float aspectOf(const Tile& tile) const noexcept;
    /// What an XMB or a Carousel is laid out from, at the eased focus.
    [[nodiscard]] RailInput railInput() const;
    /// The rectangle a tile is given in a slot of the XMB or the Carousel, so that its frame's edge
    /// is the slot's: a console's card sits 2 dp inside it, a game's fills it (navigation.md
    /// §5.3).
    [[nodiscard]] Rect railSlot(const Tile& tile, const Rect& rect) const noexcept;
    /// How far the entrance has run for a tile `distance` from the focus, or 1 when none runs.
    [[nodiscard]] float railAlpha(int distance) const noexcept;
    [[nodiscard]] Tile makeTile(library::ShelfItem item) const;
    /// The tiles whose art is wanted now: those near the canvas, as the layout owner places them.
    [[nodiscard]] SlotRange artWindow() const;
    /// Queues the art of the window's tiles that has not been, and drops queued art that left it.
    void requestArtwork(const SlotRange& window);
    void requestTile(Tile& tile, std::size_t index);
    /// Uploads up to `limit` decoded images to their tiles.
    void uploadArtwork(std::size_t limit);
    void trimResident(const SlotRange& window);
    /// The shelf's tile `index`, or the header for `headerTile`; null when there is none.
    [[nodiscard]] Tile* tileOf(std::size_t index);
    static constexpr std::size_t headerTile = std::numeric_limits<std::size_t>::max();
    /// Fills `visual` with what `tile` shows: its art, frame, names and badges.
    void fillContent(TileVisual& visual, const Tile& tile) const;
    /// Points focus at `index`, restarting the focus scale and bringing it into view.
    void focusOn(std::size_t index, int dx);
    void startEntrance();
    void releaseTextures();
    void unloadArtwork(Tile& tile);
    void drawScene();
    void drawGrid();
    void drawRail();
    /// The rectangles an XMB's or a Carousel's layout `rects` are drawn at, tile by tile, for the
    /// tiles from `first` on.
    [[nodiscard]] std::vector<Rect> railSlots(const std::vector<Rect>& rects,
                                              std::size_t first) const;
    /// What a click at `point` hits while the Guide menu, the launch panel or the layout picker is
    /// up.
    [[nodiscard]] PointerTarget pointAtModal(Vector2 point) const;
    /// What it hits on the home screen below the dock.
    [[nodiscard]] PointerTarget pointAtHome(Vector2 point) const;
    /// The dock as it stands this frame in a `width` x `height` target.
    struct DockFrame {
        DockMetrics metrics;
        DockLayout layout;
        DockStyle style;
    };
    [[nodiscard]] DockFrame dockFrame(float width, float height) const;
    void drawDock(const DockFrame& frame, float frameHeight);
    /// Makes `scene_` as big as the window.
    void ensureScene();
    [[nodiscard]] TileVisual visualFor(std::size_t slot, const Rect& rect) const;

    std::vector<Tile> tiles_;
    /// A game's tile in `tiles_`, by game id.
    std::unordered_map<std::string, std::size_t> gameTiles_;
    ImageDecoder decoder_;
    /// Replaced with the shelf, so decodes for an earlier one are dropped.
    std::uint64_t shelfSerial_{};
    std::uint64_t lastTicket_{};
    /// Tiles holding textures, the oldest first.
    std::vector<std::size_t> resident_;
    /// What stands for the folder in the XMB's left column.
    std::optional<Tile> header_;
    Platforms platforms_;
    /// Before the painters that read it.
    input::LastDevice device_;
    GlyphTextures glyphs_;
    /// The dock's icons, by `home`, `home_selected`, `library`, `library_selected`.
    GlyphTextures navIcons_;
    HomeLayout layout_;
    GridFocus focus_;
    motion::ScrollEaser scroller_;
    /// Where an XMB's or a Carousel's focus has eased to, in tiles.
    motion::VisualIndexEaser railFocus_;
    DockVisibility dockVisibility_;
    /// The section whose dock item the pointer is on.
    std::optional<library::Section> dockHover_;
    std::array<IconPop, library::allSections.size()> iconPops_{IconPop{true}, IconPop{false}};
    ModeChooser chooser_;

    Clock::time_point now_{Clock::now()};
    Clock::time_point lastTick_{now_};
    Clock::time_point focusAt_{now_};
    std::optional<Clock::time_point> pressAt_;
    std::size_t pressIndex_{};
    std::optional<Clock::time_point> entranceAt_;

    TilePainter tilePainter_;
    DockPainter dockPainter_{device_};
    RailPainter railPainter_;
    BackdropBlur blur_;
    /// The frame up to the dock, which the dock's glass blurs.
    RenderTexture2D scene_{};
    ModeChooserPainter chooserPainter_;
    PagePillPainter pillPainter_;
    PageArrowPainter arrowPainter_;
    Hud hud_{device_};
    GameMenu gameMenu_;
    GameMenuPainter gameMenuPainter_{device_};
    LaunchPanel launchPanel_;
    LaunchPanelPainter launchPanelPainter_{device_};

    config::HomeMode mode_;
    library::Section section_{library::Section::Home};
    library::LibraryMode libraryMode_{library::LibraryMode::Standard};
    int width_{};
    int height_{};
    int page_{};
    int entranceFirstStep_{};
    int entranceSpread_{};
    bool entrancePending_{false};
    bool inGame_{false};
};

} // namespace opensu::ui

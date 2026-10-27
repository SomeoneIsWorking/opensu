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

#include <SDL3/SDL_gpu.h>

#include "gpu2d/render_target.hpp"
#include "gpu2d/surface.hpp"

#include "chord_hint.hpp"
#include "chord_hint_painter.hpp"
#include "config/config.hpp"
#include "corner_hints.hpp"
#include "dock_metrics.hpp"
#include "dock_motion.hpp"
#include "dock_painter.hpp"
#include "glyph_textures.hpp"
#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "hud.hpp"
#include "image_decoder.hpp"
#include "input/chord_set.hpp"
#include "input/prompts.hpp"
#include "interface_text.hpp"
#include "library/game.hpp"
#include "library/sections.hpp"
#include "library/shelf.hpp"
#include "mapping_page.hpp"
#include "mapping_page_painter.hpp"
#include "on_screen_keyboard.hpp"
#include "on_screen_keyboard_painter.hpp"
#include "page_arrow.hpp"
#include "page_pill.hpp"
#include "paint.hpp"
#include "panel_fade.hpp"
#include "platform.hpp"
#include "pointer_target.hpp"
#include "rail_layout.hpp"
#include "rail_painter.hpp"
#include "screen_stack.hpp"
#include "section_view.hpp"
#include "tile.hpp"
#include "tile_artwork.hpp"
#include "tile_motion.hpp"
#include "tile_painter.hpp"
#include "vector_icon.hpp"
#include "volume_osd.hpp"
#include "volume_osd_painter.hpp"

namespace opensu::ui {

/// The home screen.
class Shell {
  public:
    using Clock = std::chrono::steady_clock;

    /// `assetsDir` holds the face and the icons the shell draws.
    Shell(int width, int height, config::HomeMode mode, const std::filesystem::path& assetsDir);
    ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    /// Reports a new window size and re-flows the grid.
    void setSize(int width, int height);

    /// Replaces the grid's entries, releasing their artwork, focuses `focus` and plays the
    /// entrance.
    void setShelf(std::vector<library::ShelfItem> items, std::size_t focus = 0);
    void updateShelf(std::vector<library::ShelfItem> items);

    /// Sets the card the XMB's left column shows inside a folder, standing for the folder, or
    /// clears it (navigation.md §5.4).
    void setHeader(std::optional<library::ShelfItem> header);

    /// Streams the artwork of the tiles near the canvas: queues their files to decode on the
    /// decoder's thread and uploads at most `uploads` decoded images as textures, then releases
    /// the textures of tiles far from the canvas once many are held. Once a frame, drawn or not.
    void loadArtwork(std::size_t uploads = uploadsPerFrame);

    /// Loads the artwork of the tiles near the canvas and waits until it is all in; for a still
    /// render.
    void loadArtworkNow();

    /// Draws on `device` from here on: loads the face and makes every texture there. Once, when
    /// the window is up; throws gpu2d::GpuError when no face loads.
    void attach(SDL_GPUDevice& device);

    /// Textures uploaded per frame, so a screenful of new covers spreads over a few frames.
    static constexpr std::size_t uploadsPerFrame = 4;

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

    /// Makes `section` the one shown, which sets its grid and where the dock stands. The shelf is
    /// the caller's to replace after. `revealDock` slides the dock in for a moment (iiSU
    /// `jk2.java:937`), as an L1 or R1 press does.
    void setSection(library::Section section, bool revealDock);
    /// Whether the dock stays up on Library (the picker's "Pin navigation bar" option).
    void setPinLibraryDock(bool pinned);
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

    /// Sets iiSU's icon size level (1 to 20) Library's tiles are laid out at; the grid, the XMB
    /// and the Carousel all follow it.
    void setIconSize(int level);
    [[nodiscard]] int iconSize() const noexcept {
        return iconSize_;
    }

    /// Sets how the home grid scrolls; takes effect in place.
    void setHomeMode(config::HomeMode mode);
    /// Draws everything at `factor` of its size; the layouts are drawn at 1.
    void setUiScale(float factor);
    /// The on-screen keyboard style every field opens with from now on.
    void setKeyboardStyle(input::KeyboardStyle style) noexcept {
        keyboard_.setStyle(style);
    }
    /// Shows the volume display with `level`.
    void showVolume(const VolumeLevel& level) {
        volumeOsd_.show(level, now_);
    }
    [[nodiscard]] const VolumeOsd& volumeOsd() const noexcept {
        return volumeOsd_;
    }
    /// Shows the chords of `modifier`, held long, or hides them for nothing.
    void followChordHint(std::optional<gamepad::Button> modifier) {
        chordHint_.follow(modifier, chords_, now_);
    }
    [[nodiscard]] const ChordHint& chordHint() const noexcept {
        return chordHint_;
    }
    /// Whether the chord list is drawn, fading out included.
    [[nodiscard]] bool chordHintVisible() const noexcept {
        return chordHint_.visible(now_);
    }

    /// The interface toolkit's screens: the Guide menu, Settings, a game's details, the tile
    /// menu and what opens over them.
    [[nodiscard]] ScreenStack& screens() noexcept {
        return screens_;
    }
    [[nodiscard]] const ScreenStack& screens() const noexcept {
        return screens_;
    }
    /// The on-screen keyboard docked along the bottom edge, which types into the focused field
    /// and takes the place of the nav dock and the corner hints while it is up.
    [[nodiscard]] OnScreenKeyboard& keyboard() noexcept {
        return keyboard_;
    }
    [[nodiscard]] const OnScreenKeyboard& keyboard() const noexcept {
        return keyboard_;
    }
    /// Whether a panel that takes every button is up: a toolkit screen, the keyboard or the
    /// mapping editor.
    [[nodiscard]] bool panelOpen() const noexcept {
        return screens_.active() || keyboard_.isOpen() || mapping_.isOpen();
    }

    /// Which device the prompts name. The caller feeds it; the painters read it.
    [[nodiscard]] input::LastDevice& inputDevice() noexcept {
        return prompts_.device;
    }
    /// The shortcut table the prompts name keys from; the app rebinds it.
    [[nodiscard]] input::Shortcuts& shortcuts() noexcept {
        return prompts_.shortcuts;
    }
    /// The chords in force: the shortcut table's and the running game's.
    [[nodiscard]] input::ChordSet& chords() noexcept {
        return chords_;
    }
    /// The button mapping editor, a card over the details page or the Settings screen.
    [[nodiscard]] MappingPage& mappingPage() noexcept {
        return mapping_;
    }
    [[nodiscard]] const MappingPage& mappingPage() const noexcept {
        return mapping_;
    }

    /// The pointer at `point`, or away from the window with nothing. Hovers the dock item under
    /// it and slides a hidden dock in when the pointer rests where the dock is. Returns what the
    /// pointer is on.
    PointerTarget pointAt(std::optional<Point> point);

    /// Moves the focus the D-pad moves to the element in `target`: a tile, a page, a layout card
    /// or a menu row. Reports whether it moved.
    bool focusTarget(const PointerTarget& target);
    /// Focuses slot `index` of the grid or tile `index` of the rail; reports whether it moved.
    bool focusTile(std::size_t index);
    /// Turns a Paged grid to `page`, focusing the same row's first or last slot there; reports
    /// whether it turned.
    bool focusPage(int page);

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

    /// Records one frame into `target` on `canvas`: the home screen, or while a game runs, only the
    /// Guide menus and the pages they open over a transparent frame. The caller submits it.
    void draw(Canvas& canvas, const gpu2d::Surface& target);

    /// Whether a game is running, which turns the home screen into the in-game overlay.
    void setInGame(bool inGame) noexcept {
        inGame_ = inGame;
    }
    [[nodiscard]] bool inGame() const noexcept {
        return inGame_;
    }

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
    void setHints(const HintContext& hints) noexcept {
        hud_.setHints(hints);
    }
    void setTitle(std::string title) {
        hud_.setTitle(std::move(title));
    }
    /// The breadcrumb trail the top bar shows.
    void setTrail(Trail trail) {
        hud_.setTrail(std::move(trail));
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
    /// What `item` shows, leaving `tile`'s artwork and id.
    void fillTile(Tile& tile, library::ShelfItem item) const;
    /// Starts `fade` when a panel's `open` state changed from `wasOpen`, which it then records.
    void followFade(PanelFade& fade, bool open, bool& wasOpen) const;
    /// The tiles whose art is wanted now: those near the canvas, as the layout owner places them.
    [[nodiscard]] SlotRange artWindow() const;
    /// Fills `visual` with what `tile` shows: its art, frame, names and badges.
    void fillContent(TileVisual& visual, const Tile& tile) const;
    /// Points focus at `index`, restarting the focus scale and bringing it into view.
    void focusOn(std::size_t index, int dx);
    void startEntrance();
    void drawScene(Canvas& canvas);
    /// A frame over a running game: the Guide menus, the pages they open and the chord list.
    void drawOverGame(Canvas& canvas, Point size);
    /// The held modifier's chords, while shown or fading out.
    void drawChordHint(Canvas& canvas, Point size);
    void drawGrid(Canvas& canvas);
    void drawRail(Canvas& canvas);
    /// The rectangles an XMB's or a Carousel's layout `rects` are drawn at, tile by tile, for the
    /// tiles from `first` on.
    [[nodiscard]] std::vector<Rect> railSlots(const std::vector<Rect>& rects,
                                              std::size_t first) const;
    /// What a click at `point` hits while a menu, a page or the search is up.
    [[nodiscard]] PointerTarget pointAtModal(Point point) const;
    /// Where the window's room above the docked keyboard ends, in pixels: its top while it is up,
    /// else the window's bottom.
    [[nodiscard]] float aboveKeyboard() const;
    /// What it hits on the home screen below the dock.
    [[nodiscard]] PointerTarget pointAtHome(Point point) const;
    /// The dock as it stands this frame in a `width` x `height` target.
    struct DockFrame {
        DockMetrics metrics;
        DockLayout layout;
        DockStyle style;
    };
    [[nodiscard]] DockFrame dockFrame(float width, float height) const;
    /// The dock, its glass blurring `scene`.
    void drawDock(Canvas& canvas, const gpu2d::RenderTarget& scene, const DockFrame& frame);
    /// The scene's target, as big as the window.
    [[nodiscard]] gpu2d::RenderTarget& sceneTarget();
    [[nodiscard]] SDL_GPUDevice& device() const;
    [[nodiscard]] TileVisual visualFor(std::size_t slot, const Rect& rect) const;

    std::vector<Tile> tiles_;
    /// A game's tile in `tiles_`, by game id.
    std::unordered_map<std::string, std::size_t> gameTiles_;
    /// The id the next tile takes.
    std::uint64_t nextTileId_{1};
    TileArtwork artwork_;
    /// What stands for the folder in the XMB's left column.
    std::optional<Tile> header_;
    Platforms platforms_;
    /// Before the painters that draw with it.
    InterfaceText text_;
    IconAtlas icons_;
    /// Before the painters that read it.
    input::Prompts prompts_;
    input::ChordSet chords_{prompts_.shortcuts};
    float uiScale_{1.0f};
    VolumeOsd volumeOsd_;
    ChordHint chordHint_;
    GlyphTextures glyphs_;
    HomeLayout layout_;
    GridFocus focus_;
    motion::ScrollEaser scroller_;
    /// Where an XMB's or a Carousel's focus has eased to, in tiles.
    motion::VisualIndexEaser railFocus_;
    DockVisibility dockVisibility_;
    /// The section whose dock item the pointer is on.
    std::optional<library::Section> dockHover_;
    std::array<IconPop, library::allSections.size()> iconPops_{IconPop{true}, IconPop{false}};
    /// Before the screens, whose prompts give way to it.
    OnScreenKeyboard keyboard_;
    ScreenStack screens_{prompts_, text_, keyboard_};
    PanelFade keyboardFade_{FadeSpec{150.0f, 150.0f, 1.0f, 0.0f, 1.0f}};
    OnScreenKeyboardPainter keyboardPainter_{prompts_, text_};
    MappingPage mapping_;
    PanelFade mappingFade_{FadeSpec{110.0f, 95.0f, 0.96f, 110.0f, 0.985f}};
    MappingPagePainter mappingPainter_{prompts_, text_};

    Clock::time_point now_{Clock::now()};
    Clock::time_point lastTick_{now_};
    Clock::time_point focusAt_{now_};
    std::optional<Clock::time_point> pressAt_;
    std::size_t pressIndex_{};
    std::optional<Clock::time_point> entranceAt_;

    TilePainter tilePainter_{icons_, text_};
    DockPainter dockPainter_{prompts_, text_, icons_};
    RailPainter railPainter_{text_, icons_};
    VolumeOsdPainter volumeOsdPainter_{text_};
    ChordHintPainter chordHintPainter_{prompts_, text_};
    /// The frame up to the dock, which the dock's glass blurs.
    std::optional<gpu2d::RenderTarget> scene_;
    SDL_GPUDevice* device_{};
    PagePillPainter pillPainter_;
    PageArrowPainter arrowPainter_;
    Hud hud_{prompts_, text_, icons_};

    config::HomeMode mode_;
    library::Section section_{library::Section::Home};
    library::LibraryMode libraryMode_{library::LibraryMode::Standard};
    int width_{};
    int height_{};
    int page_{};
    int entranceFirstStep_{};
    int entranceSpread_{};
    int iconSize_{defaultIconLevel};
    bool entrancePending_{false};
    bool inGame_{false};
    bool pinLibraryDock_{true};
    bool keyboardWasOpen_{false};
    bool mappingWasOpen_{false};
};

} // namespace opensu::ui

// ui — the drawn home screen.
//
// Composes the home grid's owners: layout (where cells sit), focus (which tile
// a press reaches), motion (time-based scale and scroll), the tile and pill
// painters, and the HUD around them.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "raylib.h"

#include "config/config.hpp"
#include "game_menu.hpp"
#include "game_menu_painter.hpp"
#include "glyph_textures.hpp"
#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "hud.hpp"
#include "launch_panel.hpp"
#include "launch_panel_painter.hpp"
#include "library/game.hpp"
#include "library/shelf.hpp"
#include "page_arrow.hpp"
#include "page_pill.hpp"
#include "platform.hpp"
#include "tile_motion.hpp"
#include "tile_painter.hpp"

namespace iideck::ui {

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

    /// Loads the artwork of every tile not loaded yet, from the paths the sources recorded.
    /// Drawing does this itself; it needs the GL context.
    void loadArtwork();

    /// Gives a game's tile artwork that arrived after the shelf was set.
    void setArtwork(std::string_view gameId, const std::filesystem::path& artwork);
    /// Gives a console's tile the card that arrived after the shelf was set.
    void setConsoleArtwork(std::string_view system, const std::filesystem::path& artwork);

    /// Gives the ROM tiles of a system the glyph for their frame's tab.
    void setGlyph(std::string_view system, const std::filesystem::path& glyph);

    /// Resolves an entry's platform frame. A title from a store has no console, so
    /// it is framed in that store's identity; a ROM and a console in their system's.
    [[nodiscard]] const Platform* platformFor(const library::ShelfItem& item) const;

    /// Moves focus, and reports whether it moved.
    bool moveFocus(Direction direction);

    /// Changes page in Paged mode, and reports whether it did.
    bool movePage(int delta);

    /// Focuses the first tile.
    void resetFocus();

    /// Starts the press pulse on the focused tile.
    void pressFocused();

    /// Advances motion and the toast to `now`.
    void tick(Clock::time_point now);

    /// Advances past every one-shot motion, for a still frame.
    void settle();

    /// Draws one frame: the home screen, or while a game runs, only the Guide menu over a
    /// transparent frame.
    void draw();

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
    /// Points focus at `index`, restarting the focus scale and bringing it into view.
    void focusOn(std::size_t index, int dx);
    void startEntrance();
    void releaseTextures();
    static void unloadArtwork(Tile& tile);
    void drawGrid();
    [[nodiscard]] TileVisual visualFor(std::size_t slot) const;

    config::HomeMode mode_;
    int width_{};
    int height_{};
    std::vector<Tile> tiles_;
    Platforms platforms_;
    GlyphTextures glyphs_;
    HomeLayout layout_;
    GridFocus focus_;
    int page_{};
    motion::ScrollEaser scroller_;

    Clock::time_point now_{Clock::now()};
    Clock::time_point lastTick_{now_};
    Clock::time_point focusAt_{now_};
    std::optional<Clock::time_point> pressAt_;
    std::size_t pressIndex_{};
    std::optional<Clock::time_point> entranceAt_;
    bool entrancePending_{false};
    int entranceFirstStep_{};
    int entranceSpread_{};

    TilePainter tilePainter_;
    PagePillPainter pillPainter_;
    PageArrowPainter arrowPainter_;
    Hud hud_;
    GameMenu gameMenu_;
    GameMenuPainter gameMenuPainter_;
    LaunchPanel launchPanel_;
    LaunchPanelPainter launchPanelPainter_;
    bool inGame_{false};
};

} // namespace iideck::ui

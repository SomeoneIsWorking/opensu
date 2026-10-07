// ui — the drawn home screen.
//
// Composes the home grid's owners: layout (where cells sit), focus (which tile
// a press reaches), motion (time-based scale and scroll), the tile and pill
// painters, and the HUD around them.
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

#include "config/config.hpp"
#include "grid_focus.hpp"
#include "home_layout.hpp"
#include "hud.hpp"
#include "library/game.hpp"
#include "page_pill.hpp"
#include "platform.hpp"
#include "tile_motion.hpp"
#include "tile_painter.hpp"

namespace iideck::ui {

/// One title in the grid, with the artwork loaded for it.
struct Tile {
    library::Game game;
    /// Artwork loaded as textures, or zero for none.
    Texture portrait{};
    Texture wide{};
    bool hasPortrait{false};
    bool hasWide{false};
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

    /// Replaces the catalog, releasing the artwork already loaded, and plays the entrance.
    void setCatalog(std::vector<library::Game> games);

    /// Loads every tile's artwork from the paths the sources recorded.
    void loadArtwork();

    /// Sets the platform table every tile's frame is resolved from.
    void setPlatforms(Platforms platforms);

    /// Resolves a game's platform frame. A title from a store has no console, so
    /// it is framed in that store's identity; a ROM is framed in its system's.
    [[nodiscard]] const Platform* platformFor(const library::Game& game) const;

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

    /// Draws one frame.
    void draw();

    /// Releases every texture the shell owns.
    void unloadArtwork();

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
    [[nodiscard]] const library::Game* focusedGame() const;
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
    void setSteamState(ServiceState state) noexcept {
        hud_.setSteamState(state);
    }
    void setBattery(std::optional<device::BatteryStatus> battery) noexcept {
        hud_.setBattery(battery);
    }

  private:
    /// Pixels per dp.
    [[nodiscard]] float dp() const noexcept;
    [[nodiscard]] HomeLayout computeLayout() const;
    [[nodiscard]] float sinceMs(Clock::time_point then) const noexcept;
    [[nodiscard]] float scroll() const noexcept;

    void relayout();
    /// Points focus at `index`, restarting the focus scale and bringing it into view.
    void focusOn(std::size_t index, int dx);
    void startEntrance();
    void releaseTextures();
    void drawGrid();
    [[nodiscard]] TileVisual visualFor(std::size_t slot) const;

    config::HomeMode mode_;
    int width_{};
    int height_{};
    std::vector<Tile> tiles_;
    Platforms platforms_;
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
    Hud hud_;
};

} // namespace iideck::ui

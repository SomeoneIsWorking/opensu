// ui — the drawn home screen.
//
// The reference is built from rounded, translucent shapes over a dotted ground,
// with a gradient focus ring. Everything is drawn from geometry, so the layout is
// resolution independent and the corner radius can be a fraction of tile size.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "raylib.h"

#include "library/game.hpp"

namespace iideck::ui {

/// One entry in the grid, laid out but not yet drawn.
struct Tile {
    library::Game game;
    /// Position and size in window pixels.
    Rectangle rect{};
    int columns{1};
    int rows{1};
    bool focused{false};
    /// Artwork loaded as a texture, or zero for a generated card.
    Texture portrait{};
    Texture wide{};
    bool hasPortrait{false};
    bool hasWide{false};
};

/// The palette, as 8-bit-per-channel colours.
struct Colour {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    [[nodiscard]] operator Color() const noexcept
    {
        return Color{r, g, b, a};
    }
};

namespace palette {
/// The light, low-saturation scheme the reference uses.
inline constexpr Colour ground{0xf4, 0xf3, 0xf7, 255};
inline constexpr Colour panel{0xff, 0xff, 0xff, 255};
inline constexpr Colour ink{0x2b, 0x27, 0x33, 255};
inline constexpr Colour inkSoft{0x6f, 0x68, 0x80, 255};
/// The focus ring's three iridescent stops.
inline constexpr Colour focusA{0x7c, 0x5c, 0xff, 255};
inline constexpr Colour focusB{0xff, 0x5c, 0xa8, 255};
inline constexpr Colour focusC{0x46, 0xd7, 0xc8, 255};
inline constexpr Colour dotActive{0x3d, 0xdc, 0x84, 255};
inline constexpr Colour dotInactive{0x9a, 0x95, 0xa8, 0x60};
/// A tile corner radius, as a fraction of the tile's short side. This is the
/// value the reference border pack declares.
inline constexpr float radiusFraction = 0.0625f;
} // namespace palette

/// The home screen: owns layout and draws a frame.
class Shell {
public:
    Shell(int width, int height);
    ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    /// Reports a new window size and re-flows the grid.
    void setSize(int width, int height);

    /// Replaces the catalog, releasing the artwork already loaded and loading the
    /// new entries'.
    void setCatalog(std::vector<library::Game> games);

    /// Loads every tile's artwork from the paths the sources recorded.
    void loadArtwork();

    /// Moves focus, and reports whether it moved.
    bool moveFocus(int dx, int dy);

    /// Changes page, and reports whether it did.
    bool movePage(int delta);

    /// Focuses the first tile on the first page.
    void resetFocus();

    /// Draws one frame.
    void draw();

    /// Releases every texture the shell owns.
    void unloadArtwork();

    [[nodiscard]] const std::vector<Tile>& tiles() const noexcept { return tiles_; }
    /// How many tiles ended up with a loaded portrait texture.
    [[nodiscard]] std::size_t loadedArtwork() const noexcept;
    [[nodiscard]] std::size_t focus() const noexcept { return focus_; }
    [[nodiscard]] int pageCount() const noexcept { return static_cast<int>(pages_.size()); }
    [[nodiscard]] int page() const noexcept { return page_; }
    [[nodiscard]] const library::Game* focusedGame() const;

    void setStatus(std::string text) { status_ = std::move(text); }
    void setClock(std::string text) { clock_ = std::move(text); }
    void setToast(std::string text, bool isError = false);
    void tickToast();

private:
    /// The tile area between the top bar and the footer.
    [[nodiscard]] Rectangle gridRect() const;
    /// How many square tiles fit across.
    [[nodiscard]] int columns() const;
    /// How many tile rows fit in the grid area.
    [[nodiscard]] int rows() const;
    /// The grid's capacity in cells.
    [[nodiscard]] int capacity() const;
    /// One hundredth of the window's shorter side, the layout's base unit.
    [[nodiscard]] float unit() const;

    /// Places one tile at a cell offset within a band.
    void place(std::size_t index, const Rectangle& area, int x, int y, int cellW, int gap);

    void relayout();
    void refreshFocus();
    [[nodiscard]] bool visible(std::size_t index) const;
    void releaseTextures();

    void drawDottedGround();
    void drawTopBar();
    void drawTiles();
    void drawFooter();
    void drawToast();

    [[nodiscard]] Rectangle topPillRect() const;

    int width_{};
    int height_{};
    std::vector<Tile> tiles_;
    std::vector<std::vector<std::size_t>> pages_;
    std::size_t focus_{};
    int page_{};
    int rowHeight_{};

    std::string status_;
    std::string clock_;
    std::string toast_;
    bool toastError_{false};
    int toastFrames_{};
};

} // namespace iideck::ui
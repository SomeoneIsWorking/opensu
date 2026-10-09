// home_layout — where every home grid cell sits, in canvas pixels.
//
// The geometry of iiSU's grid renderer (hx2.g, zj2, wf7/ys8) for a horizontal
// grid, in both dashboard modes. Pure arithmetic: nothing here draws.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace opensu::ui {

/// An axis-aligned rectangle in pixels.
struct Rect {
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] float right() const noexcept {
        return x + width;
    }
    [[nodiscard]] float bottom() const noexcept {
        return y + height;
    }
    [[nodiscard]] float centreX() const noexcept {
        return x + width * 0.5f;
    }
    [[nodiscard]] float centreY() const noexcept {
        return y + height * 0.5f;
    }
    /// Whether the point is inside; the left and top edges count, the right and bottom do not.
    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= x && px < right() && py >= y && py < bottom();
    }
};

/// iiSU ap6: Flow scrolls continuously, Paged shows whole pages with peeks.
enum class ScrollMode : std::uint8_t {
    Flow,
    Paged,
};

/// A grid cell in lane units: columns and rows, inclusive, on a page.
struct GridCell {
    int left{};
    int top{};
    int right{};
    int bottom{};
    int page{};
};

/// A scroll to bring one cell into view from the current offset; `dx` is the direction of travel.
struct ScrollRequest {
    std::size_t index{};
    float current{};
    int dx{};
};

/// What zj2 sizes a WiiSu page from, in pixels.
struct WiiSuPageInput {
    float width{};
    float availableHeight{};
    int rows{};
    float spacing{};
    float pageGap{};
    float peek{};
};

/// What the layout is computed from.
struct HomeLayoutInput {
    float width{};
    float height{};
    /// Pixels per dp.
    float dp{1.0f};
    std::size_t items{};
    ScrollMode mode{ScrollMode::Flow};
    /// The persisted viewport (iiSU wx2).
    int rows{3};
    int columns{4};
    /// Space the shell's own chrome takes at the top and bottom, in pixels.
    float topInset{};
    float bottomInset{};
    /// Whether empty cells are placeholder slots filling at least four pages (Home), or the grid
    /// holds only its items (the ROMs category level, navigation.md 5.2).
    bool fillSlots{true};
};

/// WiiSu's page arrows; an arrow is absent when there is no page that way (iiSU ys8.l).
struct PageArrows {
    std::optional<Rect> previous;
    std::optional<Rect> next;
};

/// One page dot.
struct PageDot {
    float x{};
    float y{};
    float radius{};
    bool active{};
};

/// The page pill and its dots (iiSU ys8.h, wf7.l).
struct PagePill {
    Rect body;
    std::vector<PageDot> dots;
    /// Haloed only on the active dot.
    float haloRadius{};
};

class HomeLayout {
  public:
    explicit HomeLayout(const HomeLayoutInput& input);

    /// The gap for a cell size: iiSU hx2.g's gap rule with its default fraction and minimum.
    [[nodiscard]] static float gapForCell(float cell) noexcept;

    /// The rows a persisted viewport becomes (iiSU ul2.F).
    [[nodiscard]] static int clampRows(int rows) noexcept;
    /// The columns a persisted viewport becomes for a horizontal grid (iiSU ul2.F).
    [[nodiscard]] static int clampColumns(int columns) noexcept;

    /// The column count WiiSu sizes a page to (iiSU zj2).
    [[nodiscard]] static int wiiSuPageColumns(const WiiSuPageInput& page) noexcept;

    [[nodiscard]] ScrollMode mode() const noexcept {
        return mode_;
    }
    [[nodiscard]] int rows() const noexcept {
        return rows_;
    }
    /// Columns in the whole strip (Flow) or on one page (Paged).
    [[nodiscard]] int columns() const noexcept {
        return columns_;
    }
    [[nodiscard]] float cellWidth() const noexcept {
        return cellWidth_;
    }
    [[nodiscard]] float cellHeight() const noexcept {
        return cellHeight_;
    }
    [[nodiscard]] float gap() const noexcept {
        return gap_;
    }
    [[nodiscard]] float paddingLeft() const noexcept {
        return paddingLeft_;
    }
    [[nodiscard]] float paddingTop() const noexcept {
        return paddingTop_;
    }
    /// Flow's edge inset: the viewport starts this far into the canvas.
    [[nodiscard]] float viewportX() const noexcept {
        return viewportX_;
    }
    [[nodiscard]] float viewportWidth() const noexcept {
        return viewportWidth_;
    }
    /// The largest scroll offset.
    [[nodiscard]] float maxScroll() const noexcept {
        return maxScroll_;
    }
    [[nodiscard]] int pageCount() const noexcept {
        return pageCount_;
    }
    /// Distance from one page's first column to the next's.
    [[nodiscard]] float pageStride() const noexcept {
        return pageStride_;
    }
    [[nodiscard]] float pageSidePadding() const noexcept {
        return pageSidePadding_;
    }
    /// Every cell the grid shows, filled or not: whole pages, at least four of them.
    [[nodiscard]] std::size_t slotCount() const noexcept;

    /// The lane cell of a slot: column-major, page by page.
    [[nodiscard]] GridCell cellOf(std::size_t index) const noexcept;
    [[nodiscard]] int pageOf(std::size_t index) const noexcept;
    /// Every slot's cell, in index order.
    [[nodiscard]] std::vector<GridCell> cells() const;

    /// Left edge of `column` on `page` in content space (iiSU qv7.y plus the page offset).
    [[nodiscard]] float columnLeft(int column, int page) const noexcept;
    /// Top edge of `row` in content space (iiSU qv7.y).
    [[nodiscard]] float rowTop(int row) const noexcept;

    /// A slot's rectangle in content space, before scrolling.
    [[nodiscard]] Rect contentRect(std::size_t index) const noexcept;
    /// A slot's rectangle on the canvas at a scroll offset.
    [[nodiscard]] Rect canvasRect(std::size_t index, float scroll) const noexcept;

    /// The slot under the point at a scroll offset, filled or not, or nothing.
    [[nodiscard]] std::optional<std::size_t> slotAt(float scroll, float x, float y) const noexcept;
    /// The first slot of `page` in `row`, or the last when `last`; the page's first slot when the
    /// row has none there.
    [[nodiscard]] std::size_t slotOnPage(int page, int row, bool last) const noexcept;
    /// The page the pill's dot or an arrow under the point turns to, or nothing.
    [[nodiscard]] std::optional<int> pageAt(int currentPage, float x, float y) const;

    /// The Flow scroll offset that keeps `index` in view after a move of `dx` columns.
    [[nodiscard]] float scrollTarget(const ScrollRequest& request) const noexcept;
    /// The Paged scroll offset that shows `page`.
    [[nodiscard]] float pageScroll(int page) const noexcept;

    /// The page pill for the current page; empty when it is not drawn.
    [[nodiscard]] PagePill pagePill(int currentPage) const;
    /// The page arrows for the current page; none outside Paged mode or with one page.
    [[nodiscard]] PageArrows pageArrows(int currentPage) const;

  private:
    /// Compactness of a small screen, 0 to 1 (iiSU ys8.h).
    [[nodiscard]] float compactness() const noexcept;
    [[nodiscard]] Rect arrowRect(bool previous) const noexcept;
    [[nodiscard]] int slotPages(std::size_t perPage) const noexcept;
    /// How many slots the grid holds: every cell of its pages, or just the items.
    [[nodiscard]] std::size_t slotsFor(std::size_t perPage) const noexcept;
    void computeMaxScroll();

    ScrollMode mode_;
    float width_;
    float height_;
    float dp_;
    std::size_t items_;
    std::size_t slots_{};
    int rows_{};
    int columns_{};
    float cellWidth_{};
    float cellHeight_{};
    float gap_{};
    float paddingLeft_{};
    float paddingTop_{};
    float viewportX_{};
    float viewportWidth_{};
    float maxScroll_{};
    int pageCount_{1};
    float pageStride_{};
    float pageSidePadding_{};
    bool fillSlots_;
};

} // namespace opensu::ui

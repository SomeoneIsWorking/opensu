#include "home_layout.hpp"

#include <algorithm>
#include <cmath>

namespace iideck::ui {
namespace {

// iiSU ix2: gapFraction 0.12, minGapPx 10.
constexpr float gapFraction = 0.12f;
constexpr float minGapPx = 10.0f;
// iiSU ul2.F: horizontal flow viewport inset 8 dp.
constexpr float horizontalFlowInsetDp = 8.0f;
// iiSU do2: WiiSu content padding 12 dp, page gap 36 dp, peek 6 dp, minPageCellScale 0.92.
constexpr float wiiSuPaddingDp = 12.0f;
constexpr float wiiSuPageGapDp = 36.0f;
constexpr float wiiSuPeekDp = 6.0f;
constexpr float wiiSuMinPageCellScale = 0.92f;
// iiSU zj2: a WiiSu page has at least 3 columns.
constexpr int wiiSuMinColumns = 3;

/// coerceIn, which is what iiSU's gk2.C is.
float clampTo(float value, float low, float high) noexcept {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

int clampTo(int value, int low, int high) noexcept {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

/// iiSU hx2.g's base padding, from the smaller viewport side.
float basePadding(float shortSide) noexcept {
    return clampTo(shortSide * 0.016f, 10.0f, 22.0f);
}

} // namespace

float HomeLayout::gapForCell(float cell) noexcept {
    // iiSU hx2.g: gmax = max(32, minGap).
    const float most = std::max(32.0f, minGapPx);
    return clampTo(clampTo(gapFraction, 0.02f, 0.18f) * cell, clampTo(minGapPx, 0.0f, most), most);
}

int HomeLayout::clampRows(int rows) noexcept {
    // iiSU ul2.F: rows 1..6.
    return clampTo(rows, 1, 6);
}

int HomeLayout::clampColumns(int columns) noexcept {
    // iiSU ul2.F: horizontal flow keeps columns minus a reduction of 0, at least 1.
    return std::max(columns, 1);
}

int HomeLayout::wiiSuPageColumns(float width, float availableHeight, int rows, float spacing,
                                 float pageGap, float peek) noexcept {
    // iiSU zj2 works in whole pixels.
    const int lanes = std::max(rows, 1);
    const int gapPx = static_cast<int>(spacing);
    const int available = std::max(static_cast<int>(availableHeight), 1) - (lanes - 1) * gapPx;
    const int cellHeight = std::max(available, 0) / lanes;
    if (cellHeight <= 0) {
        return wiiSuMinColumns;
    }
    const auto peekPx =
        static_cast<float>(clampTo(static_cast<int>(peek), cellHeight / 4, cellHeight / 2));
    const float peekFraction = peekPx / static_cast<float>(std::max(cellHeight, 1));
    const int usable = std::max(static_cast<int>(width) - static_cast<int>(pageGap) * 2, 1);
    const int minCell = std::max(static_cast<int>(clampTo(wiiSuMinPageCellScale, 0.01f, 1.0f) *
                                                  static_cast<float>(cellHeight)),
                                 1);
    // The expansion limit is unlimited without bottom navigation (iiSU do2).
    const int most = std::max((usable + gapPx) / minCell, 2);
    const auto cellAt = [&](int count) {
        return static_cast<int>(static_cast<float>(usable - (count - 1) * gapPx) /
                                (2.0f * peekFraction + static_cast<float>(count)));
    };
    int count = 2;
    while (count < most && cellAt(count + 1) >= minCell) {
        ++count;
    }
    return std::max(count, wiiSuMinColumns);
}

HomeLayout::HomeLayout(const HomeLayoutInput& input)
    : mode_{input.mode}, width_{std::max(input.width, 1.0f)}, height_{std::max(input.height, 1.0f)},
      dp_{input.dp}, items_{input.items} {
    const bool paged = mode_ == ScrollMode::Paged;
    const float pad = paged ? wiiSuPaddingDp * dp_ : 0.0f;
    // STOPGAP: WiiSu's 12 dp horizontal padding is not applied because its consumer in iiSU is
    // not traced; the top and bottom padding are applied as insets, as zj2 subtracts them.
    const float topInsetIn = input.topInset + pad;
    const float bottomInsetIn = input.bottomInset + pad;

    // iiSU hx2.g: Flow insets the viewport, clamped to 12% of the width; Paged does not.
    viewportX_ = paged ? 0.0f : clampTo(horizontalFlowInsetDp * dp_, 0.0f, width_ * 0.12f);
    viewportWidth_ = std::max(width_ - viewportX_ * 2.0f, 1.0f);
    const float shortSide = std::min(viewportWidth_, height_);
    const float p = basePadding(shortSide);
    const float top = clampTo(topInsetIn, 0.0f, height_ * 0.45f);
    const float bottom = clampTo(bottomInsetIn, 0.0f, height_ * 0.45f);
    // iiSU hx2.g: the trailing band is the base padding over a bottom inset.
    const float tail = bottom > 0.0f ? p : clampTo(height_ * 0.045f, 28.0f, 56.0f);
    const float content = std::max(height_ - top - bottom - p - tail, 1.0f);

    rows_ = clampRows(input.rows);
    const float pageGap = paged ? wiiSuPageGapDp * dp_ : 0.0f;
    const float peek = paged ? wiiSuPeekDp * dp_ : 0.0f;

    // iiSU hx2.g: the gap starts from the short side and is refined twice.
    float gap = clampTo(shortSide * 0.018f, 10.0f, 28.0f);
    for (int pass = 0; pass < 2; ++pass) {
        const float cell = std::max(
            (content - static_cast<float>(rows_ - 1) * gap) / static_cast<float>(rows_), 1.0f);
        gap = gapForCell(cell);
    }
    gap_ = gap;
    const float fitted = std::max(
        (content - static_cast<float>(rows_ - 1) * gap_) / static_cast<float>(rows_), 1.0f);
    // iiSU hx2.g: a cell is at least min(150, 8.5% of the short side) and at most max(196, H).
    const float floorSize = 0.085f * shortSide;
    cellHeight_ =
        clampTo(fitted, std::min(150.0f, std::max(floorSize, fitted)), std::max(196.0f, height_));
    // Square cells: iiSU ul2.F's aspect ratio 1.0.
    cellWidth_ = clampTo(cellHeight_, std::min(180.0f, std::max(floorSize, cellHeight_)),
                         std::max(196.0f, viewportWidth_));

    if (paged) {
        // STOPGAP: use the grid gap as zj2's spacing because the value iiSU passes is not traced.
        columns_ = clampColumns(
            wiiSuPageColumns(width_, height_ - top - bottom, rows_, gap_, pageGap, peek));
        const auto reserved = static_cast<float>(columns_);
        const float between = static_cast<float>(columns_ - 1) * gap_;
        // iiSU hx2.g: cells shrink so a whole page plus both peeks and gaps fit.
        const float peekFraction = peek / std::max(cellWidth_, 1.0f);
        const float shrunk = std::max(
            (viewportWidth_ - pageGap * 2.0f - between) / (peekFraction * 2.0f + reserved), 1.0f);
        if (shrunk < cellWidth_) {
            cellWidth_ = shrunk;
            cellHeight_ = shrunk;
        }
        const float pageWidth = reserved * cellWidth_ + between;
        // iiSU hx2.g: a page that fits is centred, else padded by peek plus gap.
        const float centred = (viewportWidth_ - pageWidth) * 0.5f;
        const float peekPad = clampTo(peek, cellWidth_ * 0.25f, cellWidth_ * 0.5f) + pageGap;
        pageSidePadding_ = centred >= 0.0f ? centred : std::max(peekPad, 0.0f);
        pageStride_ = pageWidth + pageGap;
        const std::size_t perPage =
            static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_);
        pageCount_ = std::max(1, static_cast<int>((items_ + perPage - 1) / perPage));
    } else {
        // iiSU hx2.g: a horizontal flow has as many columns as the items need.
        columns_ = std::max(1, static_cast<int>((items_ + static_cast<std::size_t>(rows_) - 1) /
                                                static_cast<std::size_t>(rows_)));
        pageCount_ = 1;
    }

    const float gridHeight =
        static_cast<float>(rows_) * cellHeight_ + static_cast<float>(rows_ - 1) * gap_;
    // iiSU hx2.g: centred in the band below the top inset, never above padding + inset.
    paddingTop_ = std::max(p + top, (height_ - top - bottom - tail - gridHeight) * 0.5f + top);
    // iiSU hx2.g: Flow's left padding is the top padding (no edge padding configured).
    paddingLeft_ = paged ? pageSidePadding_ : paddingTop_;
    computeMaxScroll();
}

void HomeLayout::computeMaxScroll() {
    if (items_ == 0) {
        maxScroll_ = 0.0f;
        return;
    }
    float right = 0.0f;
    float lastWidth = 0.0f;
    const std::size_t count = slotCount();
    for (std::size_t index = 0; index < count; ++index) {
        const Rect rect = contentRect(index);
        if (rect.right() >= right) {
            right = rect.right();
            lastWidth = rect.width;
        }
    }
    float endPadding = paddingLeft_;
    if (mode_ == ScrollMode::Flow) {
        // iiSU hx2.g: Flow may scroll until the last column is centred.
        endPadding = std::max(endPadding, (viewportWidth_ - lastWidth) * 0.5f);
    }
    maxScroll_ = std::max(right + endPadding - viewportWidth_, 0.0f);
}

std::size_t HomeLayout::slotCount() const noexcept {
    if (mode_ == ScrollMode::Paged) {
        // iiSU ou4: every empty cell up to the last page is a placeholder.
        return static_cast<std::size_t>(pageCount_) * static_cast<std::size_t>(columns_) *
               static_cast<std::size_t>(rows_);
    }
    return items_;
}

GridCell HomeLayout::cellOf(std::size_t index) const noexcept {
    // iiSU kj2.a0: horizontal flow fills column-major.
    const auto rows = static_cast<std::size_t>(rows_);
    const auto column = static_cast<int>(index / rows);
    const auto row = static_cast<int>(index % rows);
    if (mode_ == ScrollMode::Paged) {
        const int page = column / columns_;
        const int inPage = column % columns_;
        return GridCell{inPage, row, inPage, row, page};
    }
    return GridCell{column, row, column, row, 0};
}

int HomeLayout::pageOf(std::size_t index) const noexcept {
    return cellOf(index).page;
}

std::vector<GridCell> HomeLayout::cells() const {
    std::vector<GridCell> out;
    out.reserve(items_);
    for (std::size_t index = 0; index < items_; ++index) {
        out.push_back(cellOf(index));
    }
    return out;
}

Rect HomeLayout::contentRect(std::size_t index) const noexcept {
    const GridCell cell = cellOf(index);
    // iiSU zz1.Z: a tile spans span x cell + (span - 1) x gap.
    const auto spanColumns = static_cast<float>(cell.right - cell.left + 1);
    const auto spanRows = static_cast<float>(cell.bottom - cell.top + 1);
    const float pageOffset = static_cast<float>(cell.page) * pageStride_;
    return Rect{
        paddingLeft_ + pageOffset + static_cast<float>(cell.left) * (cellWidth_ + gap_),
        paddingTop_ + static_cast<float>(cell.top) * (cellHeight_ + gap_),
        spanColumns * cellWidth_ + (spanColumns - 1.0f) * gap_,
        spanRows * cellHeight_ + (spanRows - 1.0f) * gap_,
    };
}

Rect HomeLayout::canvasRect(std::size_t index, float scroll) const noexcept {
    Rect rect = contentRect(index);
    rect.x += viewportX_ - scroll;
    return rect;
}

float HomeLayout::scrollTarget(std::size_t index, float current, int dx) const noexcept {
    if (items_ == 0) {
        return 0.0f;
    }
    const Rect rect = contentRect(std::min(index, items_ - 1));
    const float viewport = viewportWidth_;
    const float most = maxScroll_;
    float left = rect.x;
    const float right = rect.right();
    if (dx == 0) {
        // iiSU hx2.L: with no direction, keep the cell inside the padding.
        if (left < current + paddingLeft_) {
            return clampTo(left - paddingLeft_, 0.0f, most);
        }
        if (right > current + viewport - paddingLeft_) {
            return clampTo(right - viewport + paddingLeft_, 0.0f, most);
        }
        return clampTo(current, 0.0f, most);
    }
    // iiSU hx2.w: keep a lead of clamp(1.85 pitch, 0.12 vp, 0.32 vp) in the direction of travel.
    const float pitch = cellWidth_ + gap_;
    const float span = right - left;
    float lead = clampTo(1.85f * pitch, 0.12f * viewport, 0.32f * viewport);
    if (span > pitch * 1.5f) {
        lead = std::max(lead, std::min(span * 0.42f, viewport * 0.42f));
    }
    lead = clampTo(lead, 0.0f, viewport * 0.5f);
    const float trailing = viewport - lead;
    const float base = clampTo(current, 0.0f, most);
    if (dx > 0 && right > base + trailing) {
        left = right - trailing;
    } else if (dx < 0 && left < base + lead) {
        left -= lead;
    } else if (left >= base) {
        left = right > base + viewport ? right - viewport : base;
    }
    return clampTo(left, 0.0f, most);
}

float HomeLayout::pageScroll(int page) const noexcept {
    // iiSU hx2.L: Paged offset is page x stride.
    return static_cast<float>(clampTo(page, 0, pageCount_ - 1)) * pageStride_;
}

PagePill HomeLayout::pagePill(int currentPage) const {
    PagePill pill;
    // iiSU nx2.p: dots only in Paged mode with more than one page.
    if (mode_ != ScrollMode::Paged || pageCount_ <= 1) {
        return pill;
    }
    // iiSU ys8.h: compactness from the short side in dp.
    const float compact = clampTo((520.0f - std::min(width_, height_) / dp_) / 160.0f, 0.0f, 1.0f);
    const float scale = 1.0f - 0.16f * compact;
    const float padding = 9.0f * dp_ * scale;
    const float slot = 10.0f * dp_ * scale;
    const float spacing = 5.5f * dp_ * scale;
    const float height = 25.0f * dp_ * scale;
    const auto count = static_cast<float>(pageCount_);
    const float content = count * slot + (count - 1.0f) * spacing;
    const float widthPx =
        clampTo(padding * 2.0f + content, 38.0f * dp_, std::max(width_ - 16.0f * dp_, 38.0f * dp_));
    const float top = clampTo((34.0f - 11.0f * compact) * dp_, 8.0f * dp_,
                              std::max(height_ - height - 8.0f * dp_, 8.0f * dp_));
    pill.body = Rect{(width_ - widthPx) * 0.5f, top, widthPx, height};
    // iiSU wf7.l: active radius 3.1 dp with a 5 dp halo, inactive 2.7 dp, all times the scale.
    pill.haloRadius = 5.0f * dp_ * scale;
    const float start = pill.body.x + (widthPx - content) * 0.5f + slot * 0.5f;
    for (int page = 0; page < pageCount_; ++page) {
        const bool active = page == currentPage;
        pill.dots.push_back(PageDot{start + static_cast<float>(page) * (slot + spacing),
                                    pill.body.centreY(), (active ? 3.1f : 2.7f) * dp_ * scale,
                                    active});
    }
    return pill;
}

} // namespace iideck::ui

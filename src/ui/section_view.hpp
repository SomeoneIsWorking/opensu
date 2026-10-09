// section_view — how a section's shelf is presented: the grid's rows and columns, or an XMB or a
// Carousel, and how many tiles it shows at once. Pure, so the shell, the section change sound and
// the tests read one rule.
#pragma once

#include <cstddef>
#include <cstdint>

#include "library/sections.hpp"

namespace opensu::ui {

/// A grid's viewport (iiSU `wx2`).
struct Viewport {
    int rows{};
    int columns{};
};

/// The grid of Home and of Library's Standard mode: 3 rows, column-major, paged horizontally. Home
/// is iiSU's Quick Access grid (`go4.java:1942`); Library's is what the app shows with "Horizontal
/// Mode" on and Rows 3 (navigation.md §5.2, `roms_standard_light_categories.png`), not the 2 x 4 of
/// `ln0.java:62-73`.
inline constexpr Viewport gridViewport{3, 4};

/// The tiles an XMB or a Carousel shows either side of the focus (iiSU `xmbNeighbors`, default 1).
inline constexpr std::size_t railNeighbours = 1;

enum class Presentation : std::uint8_t { Grid, Xmb, Carousel };

/// How a section draws its shelf, and a folder opened from it: Home is always the grid; Library
/// follows the player's mode (iiSU `gh3.d`: Xmb when the category layout is Xmb).
[[nodiscard]] Presentation presentationOf(library::Section section,
                                          library::LibraryMode mode) noexcept;

/// How many tiles a section shows at once, which sizes the domino cue (iiSU `sq1` case 2): the
/// grid's cells, or the focus and its neighbours, but never more than the shelf holds.
[[nodiscard]] std::size_t visibleTiles(library::Section section, library::LibraryMode mode,
                                       std::size_t items) noexcept;

} // namespace opensu::ui

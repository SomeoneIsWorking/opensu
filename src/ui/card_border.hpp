// card_border — the two colours a console card's border runs between, which the XMB's left column
// draws the section icon in (navigation.md §5.3).
#pragma once

#include <cstdint>
#include <span>

namespace opensu::ui {

struct Rgb {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

/// The two ends of a console card's coloured border: the colours at the middle of its top edge and
/// of its bottom edge.
struct Gradient {
    Rgb from;
    Rgb to;
};

/// Reads those from a card's RGBA pixels: the stroke is 26 of the 1024 px sprite wide, so its
/// middle is 13/1024 of the side in from the edge, averaged over a short run.
[[nodiscard]] Gradient borderColours(std::span<const std::uint8_t> rgba, int width,
                                     int height) noexcept;

} // namespace opensu::ui

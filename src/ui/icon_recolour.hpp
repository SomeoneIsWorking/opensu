// icon_recolour — gives iiSU's gamepad icon a console's colours. The icon is a white body with
// its outline, pad and buttons in a cyan to blue gradient and a darker base; the XMB's left column
// draws it in the focused console's border colours instead (navigation.md §5.3).
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

/// Recolours RGBA pixels of a `width` x `height` icon in place: white stays white, and the icon's
/// own cyan to blue gradient becomes `from` to `to`, its dark base a darker `to`. Alpha is
/// untouched.
void recolourIcon(std::span<std::uint8_t> rgba, int width, int height, Rgb from, Rgb to) noexcept;

} // namespace opensu::ui

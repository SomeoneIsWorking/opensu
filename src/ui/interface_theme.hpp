// interface_theme — the chosen interface theme's tokens (`theme::InterfaceTheme`) as the interface
// toolkit's theme: palette, spacing, shape and type. Lengths are the shell's dp, so the interface
// size scales them.
#pragma once

#include "gpu2d/ui/theme.hpp"
#include "theme/interface_theme.hpp"

namespace opensu::ui {

/// Pixels per dp: the shell's (`dp`, the window on iiSU's canvas) and the display's own.
struct Density {
    float dp{1.0f};
    float display{1.0f};
};

/// Pixels per dp for what a finger touches: never smaller than the display's own dp.
[[nodiscard]] float fingerDp(Density density) noexcept;

/// A theme's colour as the painters draw it.
[[nodiscard]] gpu2d::Colour colourOf(theme::Colour colour) noexcept;

/// The toolkit theme of `tokens` at the shell's dp, its touch lengths in the display's.
[[nodiscard]] gpu2d::ui::Theme interfaceTheme(Density density,
                                              const theme::InterfaceTheme& tokens) noexcept;

} // namespace opensu::ui

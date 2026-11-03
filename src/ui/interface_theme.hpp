// interface_theme — openSU's look as the interface toolkit's tokens: the shell's ground and ink,
// white rows on a pale pane, the dark row ink for switches, sliders and the focus ring. Lengths are
// the shell's dp, so the interface size scales them.
#pragma once

#include "gpu2d/ui/theme.hpp"

namespace opensu::ui {

/// Pixels per dp: the shell's (`dp`, the window on iiSU's canvas) and the display's own.
struct Density {
    float dp{1.0f};
    float display{1.0f};
};

/// Pixels per dp for what a finger touches: never smaller than the display's own dp.
[[nodiscard]] float fingerDp(Density density) noexcept;

/// The toolkit theme at the shell's dp, its touch lengths in the display's.
[[nodiscard]] gpu2d::ui::Theme interfaceTheme(Density density) noexcept;

} // namespace opensu::ui

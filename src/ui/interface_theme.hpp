// interface_theme — openSU's look as the interface toolkit's tokens: the shell's ground and ink,
// white rows on a pale pane, the dark row ink for switches, sliders and the focus ring. Lengths are
// the shell's dp, so the interface size scales them.
#pragma once

#include "gpu2d/ui/theme.hpp"

namespace opensu::ui {

/// The toolkit theme at `dp` pixels per dp.
[[nodiscard]] gpu2d::ui::Theme interfaceTheme(float dp) noexcept;

} // namespace opensu::ui

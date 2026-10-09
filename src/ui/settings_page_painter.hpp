// settings_page_painter — draws the Settings screen: the category list, and the focused category's
// rows on a card, clipped to it with the clip stack as they scroll.
#pragma once

#include "raylib.h"

#include "settings_page.hpp"

namespace opensu::ui {

class SettingsPagePainter {
  public:
    /// Draws `page` in `layout` over a `size` frame at `dp` pixels per dp, at opacity `alpha`.
    static void paint(const SettingsPage& page, const SettingsLayout& layout, Vector2 size,
                      float dp, float alpha);
};

} // namespace opensu::ui

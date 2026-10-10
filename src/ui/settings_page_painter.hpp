// settings_page_painter — draws the Settings screen: the category list, and the focused category's
// rows on a card, clipped to it with the clip stack as they scroll.
#pragma once

#include "raylib.h"

#include "settings_page.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class SettingsPagePainter {
  public:
    explicit SettingsPagePainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Draws `page` in `layout` over a `size` frame at `dp` pixels per dp, at opacity `alpha`.
    void paint(const SettingsPage& page, const SettingsLayout& layout, Vector2 size, float dp,
               float alpha) const;

    /// Draws one row in `box`, outlined when `focused`.
    void paintRow(const SettingsRow& row, const SettingsRowBox& box, bool focused, float dp,
                  float alpha) const;

  private:
    Typeface& typeface_;
};

} // namespace opensu::ui

// settings_page_painter — draws the Settings screen: the category list, and the focused category's
// rows on a card, clipped to it as they scroll; and the rows other pages share.
#pragma once

#include "paint.hpp"

#include "settings_page.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class SettingsPagePainter {
  public:
    explicit SettingsPagePainter(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    /// Draws `page` in `layout` over a `size` frame at `dp` pixels per dp, at opacity `alpha`.
    void paint(Canvas& canvas, const SettingsPage& page, const SettingsLayout& layout, Point size,
               float dp, float alpha) const;

    /// Draws one row in `box`, outlined when `focused` and with its control lit when `editing`.
    void paintRow(Canvas& canvas, const SettingsRow& row, const SettingsRowBox& box, bool focused,
                  float dp, float alpha, bool editing = false) const;

  private:
    Typeface& typeface_;
};

} // namespace opensu::ui

// integration_chooser_painter — draws the first-run card over the dimmed screen, after the session
// dialog's card: the title, the wrapped introduction, each option as a Settings row with its
// switch, and a Done pill with its pad glyph.
#pragma once

#include "button_glyph.hpp"
#include "integration_chooser.hpp"
#include "paint.hpp"
#include "panel_fade.hpp"
#include "settings_page_painter.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class IntegrationChooserPainter {
  public:
    IntegrationChooserPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, glyphs_{prompts, typeface}, rows_{typeface} {
    }

    /// The card's introduction.
    static constexpr const char* intro =
        "ROMs and the libretro runner are always on. Turn on what else openSU should use; "
        "Settings > Integrations changes it later.";

    /// Where the card stands in a `size` frame; what a pointer hits.
    [[nodiscard]] IntegrationChooserLayout layout(const IntegrationChooser& chooser, Point size,
                                                  float dp) const;

    void paint(Canvas& canvas, const IntegrationChooser& chooser, Point size, float dp,
               const PanelLook& look) const;

  private:
    Typeface& typeface_;
    ButtonGlyphPainter glyphs_;
    SettingsPagePainter rows_;
};

} // namespace opensu::ui

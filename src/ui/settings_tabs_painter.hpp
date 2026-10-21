// settings_tabs_painter — draws the game settings tabs as a panel down the right edge over a dimmed
// screen or game: the heading, the tab strip between the shoulder glyphs, the rows (an Override
// checkbox's as `SettingsPagePainter` draws it) and the button hints.
#pragma once

#include "paint.hpp"
#include "settings_page_painter.hpp"
#include "settings_tabs.hpp"
#include "side_panel_painter.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class SettingsTabsPainter {
  public:
    SettingsTabsPainter(const input::Prompts& prompts, Typeface& typeface) noexcept
        : typeface_{typeface}, chrome_{prompts, typeface}, rows_{typeface} {
    }

    /// Where the page's parts stand in a `width` x `height` frame; what a pointer hits.
    [[nodiscard]] TabsLayout layout(const SettingsTabs& page, float width, float height,
                                    float dp) const;

    void paint(Canvas& canvas, const SettingsTabs& page, float width, float height, float dp) const;

  private:
    void paintStrip(Canvas& canvas, const SettingsTabs& page, const TabsLayout& frame,
                    float dp) const;

    Typeface& typeface_;
    SidePanelPainter chrome_;
    SettingsPagePainter rows_;
};

} // namespace opensu::ui

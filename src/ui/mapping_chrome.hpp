// mapping_chrome — the button mapping editor's card as a chrome screen of the navigator: drawn
// with its fade, and while it is up it takes every press, since it is worked with the pad only.
#pragma once

#include <memory>

#include "gpu2d/ui/screen.hpp"

#include "chrome_panel.hpp"
#include "mapping_page.hpp"
#include "mapping_page_painter.hpp"
#include "panel_fade.hpp"

namespace opensu::ui {

class MappingChrome final : public gpu2d::ui::Screen {
  public:
    MappingChrome(const MappingPage& page, const input::Prompts& prompts,
                  gpu2d::ui::Typesetter& text)
        : Screen{"mapping", "Mapping"}, page_{page}, painter_{prompts, text} {
    }

    /// Before the navigator lays it out; follows the editor opening and closing.
    void setFrame(const ChromeFrame& frame) noexcept {
        frame_ = frame;
        fade_.follow(page_.isOpen(), frame.now);
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    const MappingPage& page_;
    MappingPagePainter painter_;
    PanelFade fade_{FadeSpec{110.0f, 95.0f, 0.96f, 110.0f, 0.985f}};
    ChromeFrame frame_;
};

} // namespace opensu::ui

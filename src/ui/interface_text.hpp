// interface_text — what openSU lends the interface toolkit to draw with: its face (hinted and
// kerned, unlike the iiSU-grid labels elsewhere) and its controller glyphs, which follow the device
// last used.
#pragma once

#include <string_view>

#include "gpu2d/ui/typesetter.hpp"

#include "button_glyph.hpp"
#include "typeface.hpp"

namespace opensu::ui {

class InterfaceText final : public gpu2d::ui::Typesetter {
  public:
    explicit InterfaceText(Typeface& typeface) noexcept : typeface_{typeface} {
    }

    [[nodiscard]] float measure(std::string_view text, float size) override;
    [[nodiscard]] float lineHeight(float size) override;
    void draw(gpu2d::Canvas& canvas, std::string_view text, gpu2d::Point topLeft, float size,
              gpu2d::Colour colour) override;

  private:
    Typeface& typeface_;
};

class InterfacePrompts final : public gpu2d::ui::Prompts {
  public:
    explicit InterfacePrompts(const ButtonGlyphPainter& glyphs) noexcept : glyphs_{glyphs} {
    }

    [[nodiscard]] float width(gpu2d::ui::Action action, float height) override;
    void draw(gpu2d::Canvas& canvas, gpu2d::ui::Action action, const gpu2d::Rect& box,
              gpu2d::Colour ink) override;

  private:
    const ButtonGlyphPainter& glyphs_;
};

} // namespace opensu::ui

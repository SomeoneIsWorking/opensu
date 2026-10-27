// The interface toolkit's navigator over stand-ins for openSU's face and glyphs: every byte half an
// em wide, a line 1.25 ems, nothing drawn. Layout, focus and the screen stack need no GPU.
#pragma once

#include <string_view>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/typesetter.hpp"

namespace opensu::test {

class FakeInterfaceText final : public gpu2d::ui::Typesetter {
  public:
    [[nodiscard]] float measure(std::string_view text, float size) override {
        return static_cast<float>(text.size()) * size * 0.5f;
    }
    [[nodiscard]] float lineHeight(float size) override {
        return size * 1.25f;
    }
    void draw(gpu2d::Canvas& /*canvas*/, std::string_view /*text*/, gpu2d::Point /*topLeft*/,
              float /*size*/, gpu2d::Colour /*colour*/) override {
    }
};

class FakeInterfacePrompts final : public gpu2d::ui::Prompts {
  public:
    [[nodiscard]] float width(gpu2d::ui::Action /*action*/, float height) override {
        return height;
    }
    void draw(gpu2d::Canvas& /*canvas*/, gpu2d::ui::Action /*action*/, const gpu2d::Rect& /*box*/,
              gpu2d::Colour /*ink*/) override {
    }
};

/// A navigator over the fakes at 1 px per dp in a 1280 x 800 window, as the shell lays out each
/// frame.
struct InterfaceRig {
    FakeInterfaceText text;
    FakeInterfacePrompts prompts;
    gpu2d::ui::Navigator navigator{text, prompts, {}};
    double now{0.0};
    gpu2d::Rect viewport{0.0f, 0.0f, 1280.0f, 800.0f};

    /// Lays the stack out after `seconds` more on the clock.
    void frame(double seconds = 0.0) {
        now += seconds;
        navigator.setTime(now);
        navigator.layout(viewport);
    }
};

} // namespace opensu::test

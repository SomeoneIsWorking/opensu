// chrome_panel — the root of one of the shell's chrome screens (the top bar, the dock, the docked
// keyboard, the mapping editor): drawn by the painter that owns its look, holding the parts the
// pointer reaches as children placed by the chrome's own geometry. A solid panel takes every
// press on it, so none falls through to the screen under it.
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/ui/layout.hpp"

namespace opensu::ui {

class ChromePanel final : public gpu2d::ui::Container {
  public:
    using Draw = std::function<void(gpu2d::ui::Painter&)>;

    explicit ChromePanel(Draw draw) : draw_{std::move(draw)} {
    }

    /// Takes the pointer over `area`, where no part takes it first.
    ChromePanel& solid(const gpu2d::Rect& area) {
        solid_ = true;
        area_ = area;
        return *this;
    }
    /// Adds `part` standing at `frame`.
    template <typename W> W& part(std::unique_ptr<W> part, const gpu2d::Rect& frame) {
        frames_.push_back(frame);
        return adopt(std::move(part));
    }

    void arrange(gpu2d::ui::LayoutContext& context, const gpu2d::Rect& frame) override;
    [[nodiscard]] bool pressable() const override {
        return solid_;
    }
    bool press(gpu2d::Point /*at*/, gpu2d::ui::ActContext& /*context*/) override {
        return false;
    }
    void paint(gpu2d::ui::Painter& painter) const override;

  private:
    Draw draw_;
    std::vector<gpu2d::Rect> frames_;
    bool solid_{false};
    gpu2d::Rect area_;
};

/// A part of a chrome a press does something with: `pressed` on a press, `hovered` when the
/// pointer moves onto it, each given the navigator's context to sound a move.
class ChromePart final : public gpu2d::ui::Widget {
  public:
    using Hook = std::function<void(gpu2d::ui::ActContext&)>;

    ChromePart(std::string id, Hook pressed, Hook hovered = {})
        : Widget{std::move(id)}, pressed_{std::move(pressed)}, hovered_{std::move(hovered)} {
    }
    [[nodiscard]] bool pressable() const override {
        return true;
    }
    bool press(gpu2d::Point /*at*/, gpu2d::ui::ActContext& context) override {
        if (pressed_) {
            pressed_(context);
        }
        return true;
    }
    void hover(gpu2d::Point /*at*/, gpu2d::ui::ActContext& context) override {
        if (hovered_) {
            hovered_(context);
        }
    }

  private:
    Hook pressed_;
    Hook hovered_;
};

/// What a chrome stands in this frame: the window, its pixels per dp and the clock.
struct ChromeFrame {
    float width{};
    float height{};
    float dp{1.0f};
    std::chrono::steady_clock::time_point now{};
};

} // namespace opensu::ui

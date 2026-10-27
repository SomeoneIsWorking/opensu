// progress_screen — a card over the dimmed screen saying what is under way: its title, a line on
// how it goes and a bar, filled when the work has a measure and sweeping when it has none. Back
// does the one thing the card offers (cancel, hide) or nothing while the work cannot be left.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

namespace opensu::ui {

/// What the card shows now.
struct ProgressCard {
    std::string title;
    std::string line;
    /// 0 to 1; nothing when the work has no measure.
    std::optional<float> fraction;
    /// What Back does, as its prompt names it; empty when Back does nothing.
    std::string leave;
};

class ProgressScreen final : public gpu2d::ui::Screen {
  public:
    /// `card` is read on every build; `leave` is what Back does while the card names it.
    ProgressScreen(std::string id, std::function<ProgressCard()> card, std::function<void()> leave)
        : Screen{std::move(id), {}}, card_{std::move(card)}, leave_{std::move(leave)} {
    }

    [[nodiscard]] std::string title() const override {
        return card_().title;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    std::function<ProgressCard()> card_;
    std::function<void()> leave_;
};

} // namespace opensu::ui

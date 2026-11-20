// details_screen — a game's details page on the interface toolkit, over its wide picture faded
// when the store has one: its cover whole beside the title, the store or system and what is known
// about it, and a pane of the launch button, focused, and the rows that act on it, each row naming
// the copy it acts on; and a list card over it: More, or Versions with every version's Play on top
// and what removes each at the bottom.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "details_controller.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class DetailsScreen final : public gpu2d::ui::Screen {
  public:
    DetailsScreen(DetailsController& controller, const ui::Chrome& chrome, ui::ScreenImages& images)
        : Screen{"details", {}}, controller_{controller}, chrome_{chrome}, images_{images} {
    }

    [[nodiscard]] std::string title() const override;
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    DetailsController& controller_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
};

class DetailsListScreen final : public gpu2d::ui::Screen {
  public:
    /// The rows the card lists now; it closes when there are none.
    using Rows = std::function<std::vector<ui::SettingsRow>()>;

    DetailsListScreen(DetailsController& controller, const ui::Chrome& chrome, std::string id,
                      std::string title, Rows rows)
        : Screen{std::move(id), std::move(title)}, controller_{controller}, chrome_{chrome},
          rows_{std::move(rows)} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    DetailsController& controller_;
    const ui::Chrome& chrome_;
    Rows rows_;
};

} // namespace opensu::app

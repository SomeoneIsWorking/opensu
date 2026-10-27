// giveaways_screen — the Giveaways screens on the interface toolkit. The list shows every offer as
// a card (art, title, store and when it ends, Owned or DLC) in a grid under the screen's title
// and how fresh the offers are; A on a card opens its details, X reads the stores again. The
// details show the art large beside the title, store, kind, when it ends and how Claim works,
// with the Claim button focused; nothing is claimed until it is pressed.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "giveaways_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class GiveawaysScreen final : public gpu2d::ui::Screen {
  public:
    GiveawaysScreen(GiveawaysController& controller, const ui::Chrome& chrome,
                    ui::ScreenImages& images);

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    GiveawaysController& controller_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
};

/// One offer's details, by its key; it closes when the offer is gone.
class GiveawayDetails final : public gpu2d::ui::Screen {
  public:
    GiveawayDetails(GiveawaysController& controller, const ui::Chrome& chrome,
                    ui::ScreenImages& images, std::string offer);

    [[nodiscard]] std::string title() const override {
        return title_;
    }
    [[nodiscard]] const std::string& offer() const noexcept {
        return offer_;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] std::string initialFocus() const override;

  private:
    GiveawaysController& controller_;
    const ui::Chrome& chrome_;
    ui::ScreenImages& images_;
    std::string offer_;
    std::string title_;
};

} // namespace opensu::app

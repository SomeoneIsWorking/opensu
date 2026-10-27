// settings_screen — the Settings screens on the interface toolkit. The root shows the categories as
// tabs down the side and the chosen one's rows beside them; a page a row opens shows its title
// where the tabs were and its rows beside it. Each row of `SettingsController`'s model becomes the
// standard widget of its kind, which behaves the same wherever it is: a switch flips on A, a
// choice steps on Left and Right and lists its options on A, a slider steps on Left and Right, a
// folder or page row opens on A, a row that throws something away asks first.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "settings_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class SettingsScreen final : public gpu2d::ui::Screen {
  public:
    SettingsScreen(SettingsController& controller, const ui::Chrome& chrome, std::string category)
        : Screen{"settings", "Settings"}, controller_{controller}, chrome_{chrome},
          category_{std::move(category)} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override {
        return tabId();
    }

    /// The chosen category's label.
    [[nodiscard]] std::string categoryLabel() const;
    /// The id of the chosen category's tab.
    [[nodiscard]] std::string tabId() const;

  private:
    SettingsController& controller_;
    const ui::Chrome& chrome_;
    std::string category_;
    std::string label_;
};

/// A page of rows a row opened: an emulator layer's settings, the list of consoles, the BIOS files.
class SettingsSubPage final : public gpu2d::ui::Screen {
  public:
    SettingsSubPage(SettingsController& controller, const ui::Chrome& chrome, std::string id)
        : Screen{std::move(id), {}}, controller_{controller}, chrome_{chrome} {
    }

    [[nodiscard]] std::string title() const override {
        return title_;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    bool act(gpu2d::ui::Action action, gpu2d::ui::ActContext& context) override;

  private:
    SettingsController& controller_;
    const ui::Chrome& chrome_;
    /// The page's title as last built; a layer's changes while its shader picker is open.
    std::string title_;
};

} // namespace opensu::app

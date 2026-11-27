// quick_setting_picker — what Customize opens over the Quick settings panel: Add a setting, which
// browses Settings' categories and pages, and Devices for its pads (B steps back out), and adds the
// picked row to a category,
// and the list of icons a tab can show.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"

#include "settings/settings.hpp"

namespace opensu::app {

class QuickSettingsPanel;

class QuickSettingPicker final : public gpu2d::ui::Screen {
  public:
    QuickSettingPicker(QuickSettingsPanel& panel, std::size_t tab, std::size_t category)
        : Screen{"quickSettings/add", "Add a setting"}, panel_{panel}, tab_{tab},
          category_{category} {
    }

    [[nodiscard]] std::string title() const override;
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;

  private:
    /// The row adding `pin` to the category, or saying it is in already.
    void addPin(gpu2d::ui::Column& rows, const settings::QuickPin& pin, const std::string& note);

    QuickSettingsPanel& panel_;
    std::size_t tab_;
    std::size_t category_;
    /// The pages opened, Settings' categories first; empty at the categories.
    std::vector<std::string> pages_;
};

class QuickIconScreen final : public gpu2d::ui::Screen {
  public:
    QuickIconScreen(QuickSettingsPanel& panel, std::size_t tab)
        : Screen{"quickSettings/icon", "Tab icon"}, panel_{panel}, tab_{tab} {
    }

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] gpu2d::Rect area(const gpu2d::Rect& viewport,
                                   const gpu2d::ui::Theme& theme) const override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }

  private:
    QuickSettingsPanel& panel_;
    std::size_t tab_;
};

} // namespace opensu::app

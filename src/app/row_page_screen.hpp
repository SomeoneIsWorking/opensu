// row_page_screen — a page of rows on the interface toolkit, outside Settings: its title beside
// the rows, or above them on a narrow screen. The rows come from the page's source each build and
// go to its actions; Library's Add page and the integration wizard's steps are these.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class RowPageScreen final : public gpu2d::ui::Screen {
  public:
    /// The page as it stands now.
    using Source = std::function<ui::SettingsCategory()>;

    RowPageScreen(std::string id, Source source, ui::RowActions& actions, const ui::Chrome& chrome)
        : Screen{std::move(id), {}}, source_{std::move(source)}, actions_{actions},
          chrome_{chrome} {
    }

    [[nodiscard]] std::string title() const override {
        return title_;
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;

  private:
    Source source_;
    ui::RowActions& actions_;
    const ui::Chrome& chrome_;
    /// The page's title as last built.
    std::string title_;
};

} // namespace opensu::app

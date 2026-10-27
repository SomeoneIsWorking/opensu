// search_screen — the search card on the interface toolkit: the field, then the results as rows,
// each opening its tile; a line says when nothing matches. B closes it, leaving the search applied.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "search_controller.hpp"
#include "ui/screen_stack.hpp"

namespace opensu::app {

class SearchScreen final : public gpu2d::ui::Screen {
  public:
    static constexpr const char* fieldId = "search/field";

    SearchScreen(SearchController& controller, const ui::Chrome& chrome)
        : Screen{"search", "Search"}, controller_{controller}, chrome_{chrome} {
    }

    /// The id of result `index`'s row.
    [[nodiscard]] static std::string resultId(std::size_t index);

    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget> build(const gpu2d::ui::Theme& theme) override;
    [[nodiscard]] bool overlay() const override {
        return true;
    }
    void paintBackdrop(gpu2d::ui::Painter& painter, const gpu2d::Rect& viewport) const override;
    [[nodiscard]] BackReply back(gpu2d::ui::ActContext& context) override;
    [[nodiscard]] std::vector<gpu2d::ui::Hint> hints() const override;
    [[nodiscard]] std::string initialFocus() const override {
        return fieldId;
    }

  private:
    SearchController& controller_;
    const ui::Chrome& chrome_;
};

} // namespace opensu::app

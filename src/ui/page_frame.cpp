#include "page_frame.hpp"

#include <utility>

namespace opensu::ui {

gpu2d::ui::List& rowsPane(gpu2d::ui::Container& parent, const gpu2d::ui::Theme& theme,
                          std::string id) {
    gpu2d::ui::Card& pane = parent.add<gpu2d::ui::Card>();
    pane.grow();
    pane.pad(gpu2d::ui::Insets::all(theme.spacing.gap * 2.0f));
    gpu2d::ui::List& list = pane.add<gpu2d::ui::List>(std::move(id), theme.spacing.gap);
    list.grow();
    return list;
}

} // namespace opensu::ui

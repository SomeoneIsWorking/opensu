#include "page_frame.hpp"

#include <utility>

namespace opensu::ui {

CardFrame tallCard(const gpu2d::ui::Theme& theme, const Chrome& chrome, float widthDp) {
    namespace tk = gpu2d::ui;
    auto root = std::make_unique<tk::Centre>(widthDp);
    root->pad(tk::Insets{theme.spacing.pad, chrome.top / theme.scale + theme.spacing.gap,
                         theme.spacing.pad, theme.spacing.pad});
    tk::Card& card = root->add<tk::Card>();
    card.grow();
    card.pad(tk::Insets::all(theme.spacing.pad)).space(theme.spacing.gap * 2.0f);
    return CardFrame{std::move(root), &card};
}

gpu2d::ui::List& rowsPane(gpu2d::ui::Container& parent, const gpu2d::ui::Theme& theme,
                          std::string id) {
    gpu2d::ui::Card& pane = parent.add<gpu2d::ui::Card>();
    pane.grow();
    pane.pad(gpu2d::ui::Insets::all(theme.spacing.gap * 2.0f));
    gpu2d::ui::List& list = pane.add<gpu2d::ui::List>(std::move(id), theme.spacing.gap);
    list.grow();
    return list;
}

void sideTabs(gpu2d::ui::Container& parent, const gpu2d::ui::Theme& theme, const std::string& id,
              std::vector<gpu2d::ui::Tabs::Tab> tabs, std::size_t chosen,
              std::function<void(std::size_t)> chose) {
    namespace tk = gpu2d::ui;
    tk::ScrollView& side = parent.add<tk::ScrollView>(id + "/scroll");
    side.fixWidth(theme.spacing.sidebarWidth);
    side.content<tk::Tabs>(id, std::move(tabs), chosen, std::move(chose), tk::Orientation::Vertical)
        .space(theme.spacing.gap);
}

} // namespace opensu::ui

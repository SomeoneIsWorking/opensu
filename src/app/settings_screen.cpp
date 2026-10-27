#include "settings_screen.hpp"

#include <algorithm>
#include <cstddef>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* tabsId = "categories";

/// The pane `category`'s rows are listed on, beside the side column.
void addRows(tk::Row& body, const tk::Theme& theme, const ui::SettingsCategory& category,
             SettingsController& controller) {
    tk::List& list = ui::rowsPane(body, theme, "rows/" + category.id);
    for (const ui::SettingsRow& row : category.rows) {
        ui::addRow(list.rows(), row, controller);
    }
}

} // namespace

std::string SettingsScreen::tabId() const {
    return std::string{tabsId} + "/" + category_;
}

std::string SettingsScreen::categoryLabel() const {
    return label_;
}

std::unique_ptr<tk::Widget> SettingsScreen::build(const tk::Theme& theme) {
    const std::vector<ui::SettingsCategory>& categories = controller_.categories();
    std::vector<tk::Tabs::Tab> tabs;
    std::vector<std::string> keys;
    tabs.reserve(categories.size());
    keys.reserve(categories.size());
    std::size_t chosen = 0;
    for (const ui::SettingsCategory& category : categories) {
        if (category.id == category_) {
            chosen = tabs.size();
        }
        tabs.push_back(tk::Tabs::Tab{category.id, category.label});
        keys.push_back(category.id);
    }
    category_ = categories[chosen].id;
    label_ = categories[chosen].label;

    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    tk::ScrollView& side = frame.body->add<tk::ScrollView>("categories/scroll");
    side.fixWidth(theme.spacing.sidebarWidth);
    side.content<tk::Tabs>(
        tabsId, std::move(tabs), chosen,
        [this, keys = std::move(keys)](std::size_t index) {
            category_ = keys[index];
        },
        tk::Orientation::Vertical);
    addRows(*frame.body, theme, categories[chosen], controller_);
    return std::move(frame.root);
}

tk::Screen::BackReply SettingsScreen::back(tk::ActContext& context) {
    // From the rows, Back goes to the categories first.
    if (!view().focused.starts_with(std::string{tabsId} + "/")) {
        context.focus(tabId());
        context.sound(tk::Sound::Move);
        return BackReply::Stay;
    }
    return controller_.leave();
}

bool SettingsScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    static_cast<void>(controller_.leave());
    return true;
}

std::vector<tk::Hint> SettingsScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"},
            tk::Hint{{tk::Action::PreviousTab, tk::Action::NextTab}, "Category"}};
}

std::unique_ptr<tk::Widget> SettingsSubPage::build(const tk::Theme& theme) {
    const ui::SettingsCategory& page = controller_.page(id());
    title_ = controller_.pageTitle(id());
    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    tk::Column& side = frame.body->add<tk::Column>();
    side.fixWidth(theme.spacing.sidebarWidth);
    side.pad(tk::Insets{theme.spacing.gap, theme.spacing.pad, 0.0f, 0.0f});
    side.add<tk::Label>(title_, tk::TextRole::Title, true);
    addRows(*frame.body, theme, page, controller_);
    return std::move(frame.root);
}

bool SettingsSubPage::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    static_cast<void>(controller_.leave());
    return true;
}

} // namespace opensu::app

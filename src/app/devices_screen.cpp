#include "devices_screen.hpp"

#include <cstddef>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* tabsId = "tabs";

} // namespace

std::string DevicesScreen::tabId() const {
    return std::string{tabsId} + "/" + controller_.chosen();
}

std::unique_ptr<tk::Widget> DevicesScreen::build(const tk::Theme& theme) {
    const std::vector<ui::SettingsCategory>& tabs = controller_.tabs();
    std::vector<tk::Tabs::Tab> labels;
    std::vector<std::string> keys;
    std::size_t chosen = 0;
    for (const ui::SettingsCategory& tab : tabs) {
        if (tab.id == controller_.chosen()) {
            chosen = labels.size();
        }
        labels.push_back(tk::Tabs::Tab{tab.id, tab.label});
        keys.push_back(tab.id);
    }
    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    tk::ScrollView& side = frame.body->add<tk::ScrollView>("tabs/scroll");
    side.fixWidth(theme.spacing.sidebarWidth);
    side.content<tk::Tabs>(
        tabsId, std::move(labels), chosen,
        [this, keys = std::move(keys)](std::size_t index) {
            controller_.choose(keys[index]);
        },
        tk::Orientation::Vertical);
    const ui::SettingsCategory& shown = tabs[chosen];
    tk::List& list = ui::rowsPane(*frame.body, theme, "rows/" + shown.id);
    for (const ui::SettingsRow& row : shown.rows) {
        ui::addRow(list.rows(), row, controller_);
    }
    return std::move(frame.root);
}

tk::Screen::BackReply DevicesScreen::back(tk::ActContext& context) {
    // From the rows, Back goes to the tabs first.
    if (!view().focused.starts_with(std::string{tabsId} + "/")) {
        context.focus(tabId());
        context.sound(tk::Sound::Move);
        return BackReply::Stay;
    }
    controller_.close();
    return BackReply::Stay;
}

bool DevicesScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    controller_.close();
    return true;
}

std::vector<tk::Hint> DevicesScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"},
            tk::Hint{{tk::Action::PreviousTab, tk::Action::NextTab}, "Tab"}};
}

} // namespace opensu::app

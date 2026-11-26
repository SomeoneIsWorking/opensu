#include "row_page_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

void addRows(tk::Container& body, const tk::Theme& theme, const ui::SettingsCategory& page,
             ui::RowActions& actions) {
    tk::List& list = ui::rowsPane(body, theme, "rows/" + page.id);
    for (const ui::SettingsRow& row : page.rows) {
        ui::addRow(list.rows(), row, actions);
    }
}

} // namespace

std::unique_ptr<tk::Widget> RowPageScreen::build(const tk::Theme& theme) {
    const ui::SettingsCategory page = source_();
    title_ = page.label;
    if (ui::narrow(theme, chrome_)) {
        ui::PageFrame<tk::Column> frame = ui::pageFrame<tk::Column>(theme, chrome_);
        frame.body->add<tk::Label>(title_, tk::TextRole::Title, true);
        addRows(*frame.body, theme, page, actions_);
        return std::move(frame.root);
    }
    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    tk::Column& side = frame.body->add<tk::Column>();
    side.fixWidth(theme.spacing.sidebarWidth);
    side.pad(tk::Insets{theme.spacing.gap, theme.spacing.pad, 0.0f, 0.0f});
    side.add<tk::Label>(title_, tk::TextRole::Title, true);
    addRows(*frame.body, theme, page, actions_);
    return std::move(frame.root);
}

tk::Screen::BackReply RowPageScreen::back(tk::ActContext& /*context*/) {
    if (left_) {
        left_();
    }
    return BackReply::Pop;
}

} // namespace opensu::app

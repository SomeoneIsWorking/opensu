#include "library_add_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "library_additions.hpp"
#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

void addRows(tk::Container& body, const tk::Theme& theme, const ui::SettingsCategory& page,
             LibraryAdditions& additions) {
    tk::List& list = ui::rowsPane(body, theme, "rows/" + page.id);
    for (const ui::SettingsRow& row : page.rows) {
        ui::addRow(list.rows(), row, additions);
    }
}

} // namespace

std::unique_ptr<tk::Widget> LibraryAddScreen::build(const tk::Theme& theme) {
    const ui::SettingsCategory page = additions_.page();
    title_ = page.label;
    if (ui::narrow(theme, chrome_)) {
        ui::PageFrame<tk::Column> frame = ui::pageFrame<tk::Column>(theme, chrome_);
        frame.body->add<tk::Label>(title_, tk::TextRole::Title, true);
        addRows(*frame.body, theme, page, additions_);
        return std::move(frame.root);
    }
    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    tk::Column& side = frame.body->add<tk::Column>();
    side.fixWidth(theme.spacing.sidebarWidth);
    side.pad(tk::Insets{theme.spacing.gap, theme.spacing.pad, 0.0f, 0.0f});
    side.add<tk::Label>(title_, tk::TextRole::Title, true);
    addRows(*frame.body, theme, page, additions_);
    return std::move(frame.root);
}

} // namespace opensu::app

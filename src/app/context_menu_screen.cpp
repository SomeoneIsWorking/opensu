#include "context_menu_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 380.0f;

} // namespace

std::unique_ptr<tk::Widget> ContextMenuScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Heading);
    tk::Column& rows = card.add<tk::Column>();
    rows.space(spacing.gap);
    for (const ui::SettingsRow& row : controller_.entries()) {
        ui::addRow(rows, row, controller_);
    }
    card.add<tk::HintBar>();
    return root;
}

void ContextMenuScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply ContextMenuScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

std::vector<tk::Hint> ContextMenuScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

} // namespace opensu::app

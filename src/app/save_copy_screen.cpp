#include "save_copy_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's widest, in dp.
constexpr float cardDp = 760.0f;

} // namespace

std::unique_ptr<tk::Widget> SaveCopyScreen::build(const tk::Theme& theme) {
    ui::CardFrame frame = ui::tallCard(theme, chrome_, cardDp);
    tk::Card& card = *frame.card;
    card.space(theme.spacing.gap);
    card.add<tk::Label>(title(), tk::TextRole::Title);
    card.add<tk::Label>(controller_.gameTitle(), tk::TextRole::Caption)
        .colour(theme.colours.inkMuted);
    tk::List& list = card.add<tk::List>("saves/rows", theme.spacing.gap);
    list.grow();
    for (const ui::SettingsRow& row : controller_.rows()) {
        ui::addRow(list.rows(), row, controller_);
    }
    card.add<tk::HintBar>();
    return std::move(frame.root);
}

void SaveCopyScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply SaveCopyScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

std::vector<tk::Hint> SaveCopyScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

} // namespace opensu::app

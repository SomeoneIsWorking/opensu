#include "guide_menu_screen.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The places' column's width, in dp.
constexpr float placesDp = 260.0f;

} // namespace

std::unique_ptr<tk::Widget> GuideMenuScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets{spacing.pad * 1.5f, spacing.pad * 1.5f, spacing.pad * 1.5f, 0.0f})
        .space(spacing.pad);
    root->add<tk::Label>(controller_.title(), tk::TextRole::Title);
    tk::List& places = root->add<tk::List>("guide/places", spacing.gap);
    places.grow();
    for (const ui::SettingsRow& row : controller_.places()) {
        ui::addRow(places.rows(), row, controller_);
    }
    root->add<tk::HintBar>();
    return root;
}

gpu2d::Rect GuideMenuScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    const float width = std::min(viewport.width, theme.px(placesDp + theme.spacing.pad * 3.0f));
    return gpu2d::Rect{viewport.x, viewport.y, width, viewport.height};
}

void GuideMenuScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

bool GuideMenuScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    controller_.close();
    return true;
}

std::vector<tk::Hint> GuideMenuScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, controller_.inGame() ? "Resume" : "Close"}};
}

std::unique_ptr<tk::Widget> GuideListScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(spacing.dialogWidth);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title);
    tk::Column& rows = card.add<tk::Column>();
    rows.space(spacing.gap);
    for (const ui::SettingsRow& row : rows_()) {
        ui::addRow(rows, row, controller_);
    }
    return root;
}

void GuideListScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    tk::paintScrim(painter, viewport);
}

} // namespace opensu::app

#include "guide_menu_screen.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The places' column and the quick card's width, in dp.
constexpr float placesDp = 220.0f;
constexpr float quickDp = 300.0f;

void paintScrim(tk::Painter& painter, const gpu2d::Rect& viewport) {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

} // namespace

std::unique_ptr<tk::Widget> GuideMenuScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Backdrop>();
    root->pad(tk::Insets{spacing.pad * 1.5f, spacing.pad * 1.5f, spacing.pad * 1.5f, 0.0f})
        .space(spacing.pad);
    root->add<tk::Label>(controller_.title(), tk::TextRole::Title);
    tk::Row& body = root->add<tk::Row>();
    body.align(tk::Align::Stretch).space(spacing.pad);
    body.grow();
    tk::List& places = body.add<tk::List>("guide/places", spacing.gap);
    places.fixWidth(placesDp);
    for (const ui::SettingsRow& row : controller_.places()) {
        ui::addRow(places.rows(), row, controller_);
    }
    tk::Column& side = body.add<tk::Column>();
    side.fixWidth(quickDp);
    tk::Card& card = side.add<tk::Card>();
    card.pad(tk::Insets::all(spacing.gap * 2.0f)).space(spacing.gap);
    for (const ui::SettingsRow& row : controller_.quick()) {
        ui::addRow(card, row, controller_);
    }
    side.add<tk::Spacer>();
    root->add<tk::HintBar>().fixHeight(spacing.hintBarHeight);
    return root;
}

gpu2d::Rect GuideMenuScreen::area(const gpu2d::Rect& viewport, const tk::Theme& theme) const {
    const float width =
        std::min(viewport.width, theme.px(placesDp + quickDp + theme.spacing.pad * 4.0f));
    return gpu2d::Rect{viewport.x, viewport.y, width, viewport.height};
}

void GuideMenuScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    paintScrim(painter, viewport);
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

std::string GuideMenuScreen::initialFocus() const {
    // Empty lands on the first place.
    return landing_ == GuideMenuController::Landing::Volume ? "guide.volume" : std::string{};
}

std::unique_ptr<tk::Widget> GuidePowerScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(spacing.dialogWidth);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title);
    tk::Column& rows = card.add<tk::Column>();
    rows.space(spacing.gap);
    for (const ui::SettingsRow& row : controller_.power()) {
        ui::addRow(rows, row, controller_);
    }
    return root;
}

void GuidePowerScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    paintScrim(painter, viewport);
}

} // namespace opensu::app

#include "pad_check_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/pad_preview.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* guide = "Let go: each dot should rest in the middle. Turn the sticks: they "
                              "should reach the circle all round. Pull the triggers: the bars "
                              "should fill. The faint marks are what the pad reads by itself.";

} // namespace

std::unique_ptr<tk::Widget> PadCheckScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(spacing.dialogWidth);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::symmetric(spacing.pad * 1.5f, spacing.pad * 1.4f)).space(spacing.pad);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    card.add<tk::Label>(line_, tk::TextRole::Body, true).colour(theme.colours.inkMuted);
    if (const std::optional<gamepad::PadAxes> axes = hooks_.axes()) {
        card.add<ui::PadPreview>(*axes);
    }
    card.add<tk::Label>(guide, tk::TextRole::Caption, true).colour(theme.colours.inkMuted);
    tk::ButtonRow& buttons = card.add<tk::ButtonRow>();
    buttons.space(spacing.gap * 2.0f);
    buttons.add<tk::Button>("pad.check.keep", "Keep", hooks_.keep);
    buttons.add<tk::Button>("pad.check.again", "Calibrate again", hooks_.again);
    if (hooks_.undo) {
        buttons.add<tk::Button>("pad.check.undo", "Undo", hooks_.undo);
    }
    card.add<tk::HintBar>();
    return root;
}

void PadCheckScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply PadCheckScreen::back(tk::ActContext& /*context*/) {
    hooks_.keep();
    return BackReply::Stay;
}

std::vector<tk::Hint> PadCheckScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Keep"}};
}

} // namespace opensu::app

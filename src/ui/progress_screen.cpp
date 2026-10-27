#include "progress_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

} // namespace

std::unique_ptr<tk::Widget> ProgressScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    const ProgressCard now = card_();
    auto root = std::make_unique<tk::Centre>(spacing.dialogWidth);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::symmetric(spacing.pad * 1.5f, spacing.pad * 1.4f)).space(spacing.pad);
    card.add<tk::Label>(now.title, tk::TextRole::Title, true);
    card.add<tk::Label>(now.line, tk::TextRole::Body, true).colour(theme.colours.inkMuted);
    card.add<tk::Progress>(now.fraction);
    if (!now.leave.empty()) {
        card.add<tk::HintBar>();
    }
    return root;
}

void ProgressScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply ProgressScreen::back(tk::ActContext& /*context*/) {
    if (!card_().leave.empty() && leave_) {
        leave_();
    }
    return BackReply::Stay;
}

std::vector<tk::Hint> ProgressScreen::hints() const {
    const std::string leave = card_().leave;
    if (leave.empty()) {
        return {};
    }
    return {tk::Hint{{tk::Action::Back}, leave}};
}

} // namespace opensu::ui

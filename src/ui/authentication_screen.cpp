#include "authentication_screen.hpp"

#include "entry_screen.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

} // namespace

std::unique_ptr<tk::Widget> AuthenticationScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    const AuthenticationCard now = authenticating_.card();
    auto root = std::make_unique<tk::Centre>(entryCardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(now.title, tk::TextRole::Title, true);
    if (!now.note.empty()) {
        card.add<tk::Label>(now.note, tk::TextRole::Caption, true);
    }
    if (!now.field) {
        card.add<tk::Progress>(std::nullopt);
        card.add<tk::HintBar>();
        return root;
    }
    card.add<tk::TextField>(fieldId, now.field->label, authenticating_.text(),
                            authenticating_.changed)
        .mask(now.field->secret)
        .limit(authenticating_.mostBytes)
        .onSubmit(authenticating_.submitted);
    // An edited field's prompts are the docked keyboard's row, so the card leaves no room for any.
    if (view().editing != fieldId) {
        card.add<tk::HintBar>();
    }
    return root;
}

void AuthenticationScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply AuthenticationScreen::back(tk::ActContext& /*context*/) {
    authenticating_.cancelled();
    return BackReply::Stay;
}

std::vector<tk::Hint> AuthenticationScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Cancel"}};
}

} // namespace opensu::ui

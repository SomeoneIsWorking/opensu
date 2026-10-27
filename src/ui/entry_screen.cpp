#include "entry_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::ui {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 440.0f;

} // namespace

std::unique_ptr<tk::Widget> EntryScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    if (!entry_.note.empty()) {
        card.add<tk::Label>(entry_.note, tk::TextRole::Caption, true);
    }
    card.add<tk::TextField>(fieldId, entry_.label, entry_.text(), entry_.changed)
        .placeholder(entry_.placeholder)
        .mask(entry_.secret)
        .limit(entry_.mostBytes)
        .onSubmit(entry_.submitted);
    card.add<tk::HintBar>().fixHeight(spacing.hintBarHeight);
    return root;
}

void EntryScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply EntryScreen::back(tk::ActContext& /*context*/) {
    entry_.cancelled();
    return BackReply::Stay;
}

std::vector<tk::Hint> EntryScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Cancel"}};
}

} // namespace opensu::ui

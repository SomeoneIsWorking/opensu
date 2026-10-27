#include "layout_picker_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/layout_sketch.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp (navigation.md §5.2: iiSU's panel is 523.6 dp wide).
constexpr float cardDp = 524.0f;

} // namespace

std::unique_ptr<tk::Widget> LayoutPickerScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Heading);
    tk::ScrollView& scroll = card.add<tk::ScrollView>("options/scroll");
    scroll.grow();
    tk::Column& rows = scroll.content<tk::Column>();
    rows.space(spacing.gap);
    rows.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    if (controller_.inLibrary()) {
        tk::Row& cards = rows.add<tk::Row>();
        cards.align(tk::Align::Stretch).space(spacing.gap * 2.0f);
        for (const library::LibraryMode mode : library::allLibraryModes) {
            tk::CardButton& button = cards.add<tk::CardButton>(
                layoutCardId(mode),
                [this, mode] {
                    controller_.chooseMode(mode);
                },
                "Use");
            button.mark(mode == controller_.mode());
            button.grow();
            button.pad(tk::Insets::all(spacing.gap * 1.5f)).space(spacing.gap);
            button.add<ui::LayoutSketch>(mode);
            button.add<tk::Label>(std::string{library::label(mode)}, tk::TextRole::Body);
        }
    }
    for (const ui::SettingsRow& row : controller_.rows()) {
        ui::addRow(rows, row, controller_);
    }
    card.add<tk::HintBar>();
    return root;
}

void LayoutPickerScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply LayoutPickerScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

bool LayoutPickerScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    controller_.close();
    return true;
}

std::vector<tk::Hint> LayoutPickerScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

std::string LayoutPickerScreen::initialFocus() const {
    // Empty lands on the first row.
    return controller_.inLibrary() ? layoutCardId(controller_.mode()) : std::string{};
}

} // namespace opensu::app

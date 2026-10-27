#include "first_run_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 600.0f;
constexpr const char* intro =
    "ROMs and the libretro runner are always on. Turn on what else openSU should use; Settings > "
    "Integrations changes it later.";

} // namespace

std::unique_ptr<tk::Widget> FirstRunScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    card.add<tk::Label>(intro, tk::TextRole::Body, true).colour(theme.colours.inkMuted);
    tk::ScrollView& scroll = card.add<tk::ScrollView>("firstRun/scroll");
    scroll.grow();
    tk::Column& rows = scroll.content<tk::Column>();
    rows.space(spacing.gap);
    rows.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    for (const ui::SettingsRow& row : controller_.rows()) {
        ui::addRow(rows, row, controller_);
    }
    tk::Row& foot = card.add<tk::Row>();
    foot.add<tk::Spacer>();
    foot.add<tk::Button>(
        std::string{firstRunDoneId}, "Done",
        [this] {
            controller_.done();
        },
        tk::ButtonKind::Primary);
    card.add<tk::HintBar>();
    return root;
}

void FirstRunScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

} // namespace opensu::app

#include "cover_picker_screen.hpp"

#include <cstddef>
#include <string>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/image_view.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's widest, in dp, and the covers across it.
constexpr float cardDp = 1040.0f;
constexpr std::size_t columns = 5;

} // namespace

std::unique_ptr<tk::Widget> CoverPickerScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap);
    card.add<tk::Label>(title(), tk::TextRole::Title);
    card.add<tk::Label>(controller_.status(), tk::TextRole::Caption).colour(theme.colours.inkMuted);
    tk::ScrollView& scroll = card.add<tk::ScrollView>("covers/scroll");
    scroll.grow();
    tk::Grid& grid = scroll.content<tk::Grid>("covers", columns);
    grid.space(spacing.pad * 0.75f);
    grid.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    const std::vector<CoverPickerController::Pick>& picks = controller_.picks();
    for (std::size_t i = 0; i < picks.size(); ++i) {
        tk::CardButton& cell = grid.add<tk::CardButton>(
            "cover/" + std::to_string(i),
            [this, i] {
                controller_.choose(i);
            },
            "Choose");
        cell.mark(i == 0);
        cell.pad(tk::Insets::all(spacing.gap * 1.5f)).space(spacing.gap);
        cell.add<ui::ImageView>(images_, picks[i].file, 1.0f).whole(tk::Align::Centre);
        cell.add<tk::Label>(picks[i].label, tk::TextRole::Caption).colour(theme.colours.inkMuted);
    }
    return root;
}

void CoverPickerScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply CoverPickerScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

bool CoverPickerScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Tertiary || !controller_.disconnected()) {
        return false;
    }
    controller_.connect();
    return true;
}

std::vector<tk::Hint> CoverPickerScreen::hints() const {
    std::vector<tk::Hint> hints{tk::Hint{{tk::Action::Back}, "Close"}};
    if (controller_.disconnected()) {
        hints.push_back(tk::Hint{{tk::Action::Tertiary}, "Connect SteamGridDB"});
    }
    return hints;
}

} // namespace opensu::app

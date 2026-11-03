#include "game_match_screen.hpp"

#include <cstddef>
#include <string>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/image_view.hpp"
#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's widest, in dp, and the covers across it.
constexpr float cardDp = 1040.0f;
constexpr std::size_t columns = 4;
/// A Steam library portrait's width over its height, and a box art's, which varies by system.
constexpr float portraitAspect = 2.0f / 3.0f;
constexpr float boxArtAspect = 1.0f;

} // namespace

std::unique_ptr<tk::Widget> GameMatchScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    ui::CardFrame frame = ui::tallCard(theme, chrome_, cardDp);
    tk::Card& card = *frame.card;
    card.space(spacing.gap);
    card.add<tk::Label>(title(), tk::TextRole::Title);
    card.add<tk::Label>(controller_.status(), tk::TextRole::Caption).colour(theme.colours.inkMuted);
    tk::ScrollView& scroll = card.add<tk::ScrollView>("match/scroll");
    scroll.grow();
    tk::Grid& grid = scroll.content<tk::Grid>("matches", columns);
    grid.space(spacing.pad * 0.75f);
    grid.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    const std::vector<GameMatchController::Candidate>& candidates = controller_.candidates();
    const bool boxArt = controller_.kind() == library::MatchKind::LibretroTitle;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const GameMatchController::Candidate& candidate = candidates[i];
        tk::CardButton& cell = grid.add<tk::CardButton>(
            "match/" + std::to_string(i),
            [this, i] {
                controller_.choose(i);
            },
            "Match");
        cell.mark(candidate.key == controller_.current());
        cell.pad(tk::Insets::all(spacing.gap * 1.5f)).space(spacing.gap);
        cell.add<ui::ImageView>(images_, candidate.cover, boxArt ? boxArtAspect : portraitAspect)
            .whole(tk::Align::Centre)
            .loading([complete = candidate.complete] {
                return !complete;
            });
        cell.add<tk::Label>(candidate.title, tk::TextRole::Body, true);
        if (boxArt) {
            continue;
        }
        cell.add<tk::Label>(candidate.year.empty()
                                ? "Steam app " + candidate.key
                                : candidate.year + " · Steam app " + candidate.key,
                            tk::TextRole::Caption)
            .colour(theme.colours.inkMuted);
    }
    card.add<tk::HintBar>();
    return std::move(frame.root);
}

void GameMatchScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply GameMatchScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

bool GameMatchScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Secondary) {
        return false;
    }
    controller_.searchAnother();
    return true;
}

std::vector<tk::Hint> GameMatchScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"},
            tk::Hint{{tk::Action::Secondary}, "Search another name"}};
}

} // namespace opensu::app

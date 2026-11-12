#include "lobby_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

using Step = online::LobbyFinder::Step;

constexpr float cardDp = 640.0f;

/// "1 of 2".
std::string seatsOf(const online::LobbyFinder::Game& game) {
    return std::to_string(game.players) + " of " + std::to_string(game.capacity);
}

} // namespace

std::string LobbyScreen::gameId(std::size_t index) {
    return std::string{screenId} + "/game/" + std::to_string(index);
}

std::unique_ptr<tk::Widget> LobbyScreen::build(const tk::Theme& theme) {
    const OnlinePlay::Lobbies lobbies = online_.lobbies();
    // A host is being asked, or has answered: nothing else may be picked.
    const bool busy = !lobbies.pickable;
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    if (!lobbies.note.empty()) {
        tk::Label& note = card.add<tk::Label>(lobbies.note, tk::TextRole::Body, true);
        if (lobbies.step == Step::Failed) {
            note.colour(theme.colours.danger);
        }
    }
    if (!lobbies.games.empty()) {
        tk::List& list = card.add<tk::List>(listId, spacing.gap);
        list.grow();
        for (std::size_t i = 0; i < lobbies.games.size(); ++i) {
            const online::LobbyFinder::Game& game = lobbies.games[i];
            list.rows()
                .add<tk::Item>(gameId(i), tk::Item::Text{.title = game.title, .subtitle = {}})
                .value(seatsOf(game))
                .onPress(
                    [this, i] {
                        online_.pickLobby(i);
                    },
                    "Join")
                .disable(busy);
        }
    }
    tk::ButtonRow& buttons = card.add<tk::ButtonRow>();
    buttons.space(spacing.gap * 2.0f);
    if (!busy) {
        buttons.add<tk::Button>(refreshId, "Refresh", [this] {
            online_.refreshLobbies();
        });
    }
    buttons.add<tk::Button>(closeId, "Close", [this] {
        online_.closeLobbies();
    });
    // Games that just became pickable take the focus, so A joins the first and never refreshes.
    const bool offered = !busy && !lobbies.games.empty();
    if (offered && !offered_) {
        view().focused = gameId(0);
    }
    offered_ = offered;
    card.add<tk::HintBar>();
    return root;
}

void LobbyScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply LobbyScreen::back(tk::ActContext& /*context*/) {
    online_.closeLobbies();
    return BackReply::Stay;
}

std::vector<tk::Hint> LobbyScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

} // namespace opensu::app

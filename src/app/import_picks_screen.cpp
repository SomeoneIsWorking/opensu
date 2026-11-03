#include "import_picks_screen.hpp"

#include <utility>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "config/config.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 640.0f;

std::string countLine(std::size_t chosen, std::size_t found) {
    return std::to_string(chosen) + " of " + std::to_string(found) + " chosen";
}

} // namespace

std::string importGameId(std::size_t index) {
    return std::string{importPicksId} + "/game/" + std::to_string(index);
}

std::unique_ptr<tk::Widget> ImportPicksScreen::build(const tk::Theme& theme) {
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad * 1.5f)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(title(), tk::TextRole::Title, true);
    card.add<tk::Label>(countLine(picks_.chosenCount(), picks_.found().size()), tk::TextRole::Body)
        .colour(theme.colours.inkMuted);
    tk::Row& tools = card.add<tk::Row>();
    tools.space(spacing.gap);
    tools.add<tk::Button>(std::string{importAllId}, "Select all", [this] {
        picks_.chooseAll();
    });
    tools.add<tk::Button>(std::string{importNoneId}, "Deselect all", [this] {
        picks_.chooseNone();
    });
    tools.add<tk::Button>(std::string{importUndoId}, "Undo", [this] {
        undo();
    });
    tools.add<tk::Spacer>();
    tools.add<tk::Button>(
        std::string{importDoneId}, "Import selected",
        [this] {
            import();
        },
        tk::ButtonKind::Primary);
    tk::ScrollView& scroll = card.add<tk::ScrollView>(std::string{importPicksId} + "/scroll");
    scroll.grow();
    tk::Column& rows = scroll.content<tk::Column>();
    rows.space(spacing.gap);
    rows.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    for (std::size_t i = 0; i < picks_.found().size(); ++i) {
        const library::own::Entry& entry = picks_.found()[i];
        rows.add<tk::Toggle>(importGameId(i),
                             tk::Item::Text{.title = entry.title,
                                            .subtitle = entry.file.empty()
                                                            ? std::string{}
                                                            : config::shownPath(entry.file, home_)},
                             picks_.chosen(i), [this, i](bool on) {
                                 picks_.choose(i, on);
                             });
    }
    card.add<tk::HintBar>();
    return root;
}

void ImportPicksScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

std::vector<tk::Hint> ImportPicksScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Cancel"}};
}

void ImportPicksScreen::undo() {
    if (!picks_.undo()) {
        hooks_.say("Nothing to undo", false);
    }
}

void ImportPicksScreen::import() {
    std::vector<library::own::Entry> picked = picks_.picked();
    if (picked.empty()) {
        hooks_.say("Choose a game to import first", true);
        return;
    }
    close();
    hooks_.imported(std::move(picked));
}

} // namespace opensu::app

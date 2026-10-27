#include "search_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// The card's width, in dp.
constexpr float cardDp = 560.0f;

} // namespace

std::string SearchScreen::resultId(std::size_t index) {
    return "search/result/" + std::to_string(index);
}

std::unique_ptr<tk::Widget> SearchScreen::build(const tk::Theme& theme) {
    ui::CardFrame frame = ui::tallCard(theme, chrome_, cardDp);
    tk::Card& card = *frame.card;
    card.add<tk::TextField>(fieldId, "Search", controller_.text(),
                            [this](std::string text) {
                                controller_.edit(std::move(text));
                            })
        .placeholder("ROMs, consoles, apps, collections, and folders")
        .limit(searchMostLength)
        .onSubmit(
            [this] {
                controller_.toResults();
            },
            "Results");
    const std::vector<SearchResult>& results = controller_.results();
    if (results.empty()) {
        card.add<tk::Label>(controller_.searching() ? "Nothing matches" : "Type to search",
                            tk::TextRole::Caption);
    }
    tk::List& list = card.add<tk::List>("search/results", theme.spacing.gap);
    list.grow();
    for (std::size_t i = 0; i < results.size(); ++i) {
        list.rows()
            .add<tk::Item>(resultId(i), tk::Item::Text{results[i].title, results[i].detail})
            .onPress(
                [this, i] {
                    controller_.openResult(i);
                },
                "Open");
    }
    card.add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return std::move(frame.root);
}

void SearchScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply SearchScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

std::vector<tk::Hint> SearchScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Close"}};
}

} // namespace opensu::app

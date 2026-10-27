#include "details_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/image_view.hpp"
#include "ui/page_frame.hpp"
#include "ui/row_widgets.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// A box cover's width over its height.
constexpr float coverAspect = 2.0f / 3.0f;
/// The cover's share of the room beside the action pane, against the facts' 1.
constexpr float coverShare = 0.6f;
/// The action pane's width, in dp.
constexpr float actionsDp = 380.0f;

void addFact(tk::Column& facts, const tk::Theme& theme, const ui::SettingsRow& fact) {
    tk::Column& pair = facts.add<tk::Column>();
    pair.space(theme.spacing.gap * 0.5f);
    pair.add<tk::Label>(fact.label, tk::TextRole::Caption).colour(theme.colours.inkMuted);
    pair.add<tk::Label>(fact.value, tk::TextRole::Body, true);
}

} // namespace

std::string DetailsScreen::title() const {
    const ui::GameDetails* details = controller_.details();
    return details == nullptr ? std::string{} : details->title;
}

std::unique_ptr<tk::Widget> DetailsScreen::build(const tk::Theme& theme) {
    ui::PageFrame<tk::Row> frame = ui::pageFrame<tk::Row>(theme, chrome_);
    const ui::GameDetails* details = controller_.details();
    if (details == nullptr) {
        close();
        return std::move(frame.root);
    }
    tk::Column& art = frame.body->add<tk::Column>();
    art.grow(coverShare);
    art.add<ui::ImageView>(images_, details->art, coverAspect).whole(tk::Align::Start).grow();

    tk::Column& about = frame.body->add<tk::Column>();
    about.grow();
    about.space(theme.spacing.gap);
    about.add<tk::Label>(details->title, tk::TextRole::Title, true);
    about.add<tk::Label>(details->badge, tk::TextRole::Caption).colour(theme.colours.inkMuted);
    tk::ScrollView& scroll = about.add<tk::ScrollView>("details/facts");
    scroll.grow();
    tk::Column& facts = scroll.content<tk::Column>();
    facts.pad(tk::Insets{0.0f, theme.spacing.pad, 0.0f, 0.0f}).space(theme.spacing.pad * 0.75f);
    for (const ui::SettingsRow& fact : details->facts) {
        addFact(facts, theme, fact);
    }

    tk::Card& pane = frame.body->add<tk::Card>();
    pane.fixWidth(actionsDp);
    pane.pad(tk::Insets::all(theme.spacing.gap * 2.0f));
    tk::List& list = pane.add<tk::List>("details/actions", theme.spacing.gap);
    list.grow();
    list.rows().add<tk::Button>(
        std::string{ui::detailsLaunchId}, details->launch,
        [this] {
            controller_.launch();
        },
        tk::ButtonKind::Primary);
    for (const ui::SettingsRow& row : details->actions) {
        ui::addRow(list.rows(), row, controller_);
    }
    return std::move(frame.root);
}

tk::Screen::BackReply DetailsScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

std::vector<tk::Hint> DetailsScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"}};
}

std::string DetailsScreen::initialFocus() const {
    return std::string{ui::detailsLaunchId};
}

} // namespace opensu::app

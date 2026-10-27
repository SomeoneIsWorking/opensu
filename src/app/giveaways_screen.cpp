#include "giveaways_screen.hpp"

#include <utility>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "library/shelf.hpp"
#include "ui/image_view.hpp"
#include "ui/offer_card.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr const char* claimId = "claim";
constexpr std::size_t columns = 3;

/// The screen's frame: the top bar's room, `body`, then the hint bar.
struct Frame {
    std::unique_ptr<tk::Backdrop> root;
    tk::Column* body{};
};

Frame frameFor(const tk::Theme& theme, const ui::Chrome& chrome) {
    Frame frame{std::make_unique<tk::Backdrop>(), nullptr};
    frame.root->space(theme.spacing.gap);
    frame.root->add<tk::Spacer>(0.0f).fixHeight(chrome.top / theme.scale);
    tk::Column& body = frame.root->add<tk::Column>();
    body.pad(tk::Insets{theme.spacing.pad * 2.0f, 0.0f, theme.spacing.pad * 2.0f, 0.0f})
        .space(theme.spacing.pad);
    body.grow();
    frame.body = &body;
    frame.root->add<tk::HintBar>().fixHeight(theme.spacing.hintBarHeight);
    return frame;
}

std::string kindText(const giveaways::Offer& offer) {
    return offer.kind == giveaways::OfferKind::Dlc ? "DLC: needs the base game" : "Game";
}

/// What Claim does for the offer's store, for the player.
std::string claimText(const giveaways::Offer& offer) {
    switch (offer.store) {
    case library::Source::Steam:
        return "Claim opens its store page in Steam when openSU's Steam is running, else in your "
               "browser; add it to your account there.";
    case library::Source::Epic:
        return "Claim opens its page on the Epic Games Store in your browser; sign in there and "
               "press Get.";
    case library::Source::Gog:
        return "Claim opens GOG's page in your browser; add it to your library there with your GOG "
               "account.";
    case library::Source::Rom:
        break;
    }
    return {};
}

} // namespace

GiveawaysScreen::GiveawaysScreen(GiveawaysController& controller, const ui::Chrome& chrome,
                                 ui::ScreenImages& images)
    : Screen{"giveaways", std::string{library::giveawaysName}}, controller_{controller},
      chrome_{chrome}, images_{images} {
}

std::unique_ptr<tk::Widget> GiveawaysScreen::build(const tk::Theme& theme) {
    Frame frame = frameFor(theme, chrome_);
    tk::Row& heading = frame.body->add<tk::Row>();
    heading.add<tk::Label>(std::string{library::giveawaysName}, tk::TextRole::Title);
    heading.add<tk::Spacer>();
    heading.add<tk::Label>(controller_.status(), tk::TextRole::Caption)
        .colour(theme.colours.inkMuted);
    const std::vector<giveaways::ShownOffer>& offers = controller_.offers();
    if (offers.empty()) {
        frame.body
            ->add<tk::Label>("No free-to-keep games right now. They are checked a few "
                             "times a day; X checks again.",
                             tk::TextRole::Body, true)
            .colour(theme.colours.inkMuted);
        return std::move(frame.root);
    }
    tk::ScrollView& scroll = frame.body->add<tk::ScrollView>("offers/scroll");
    scroll.grow();
    tk::Grid& grid = scroll.content<tk::Grid>("offers", columns);
    grid.space(theme.spacing.pad);
    grid.pad(tk::Insets::all(theme.shape.focusRing + theme.shape.focusGap));
    for (const giveaways::ShownOffer& shown : offers) {
        const giveaways::Offer& offer = shown.offer;
        const std::string key = giveaways::key(offer);
        ui::OfferCardText text{offer.title, controller_.lineOf(offer), {}, false};
        if (shown.owned) {
            text.badge = "Owned";
            text.strongBadge = true;
        } else if (offer.kind == giveaways::OfferKind::Dlc) {
            text.badge = "DLC";
        }
        grid.add<ui::OfferCard>("offer/" + key, images_, shown.art, std::move(text), [this, key] {
            controller_.details(key);
        });
    }
    return std::move(frame.root);
}

bool GiveawaysScreen::act(tk::Action action, tk::ActContext& context) {
    if (action != tk::Action::Secondary) {
        return false;
    }
    context.sound(tk::Sound::Activate);
    controller_.refresh();
    return true;
}

std::vector<tk::Hint> GiveawaysScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"}, tk::Hint{{tk::Action::Secondary}, "Check again"}};
}

GiveawayDetails::GiveawayDetails(GiveawaysController& controller, const ui::Chrome& chrome,
                                 ui::ScreenImages& images, std::string offer)
    : Screen{"giveaway", {}}, controller_{controller}, chrome_{chrome}, images_{images},
      offer_{std::move(offer)} {
}

std::string GiveawayDetails::initialFocus() const {
    return claimId;
}

std::unique_ptr<tk::Widget> GiveawayDetails::build(const tk::Theme& theme) {
    Frame frame = frameFor(theme, chrome_);
    const giveaways::ShownOffer* shown = controller_.find(offer_);
    if (shown == nullptr) {
        // The offer ended or was hidden while its details were open.
        close();
        return std::move(frame.root);
    }
    const giveaways::Offer& offer = shown->offer;
    title_ = offer.title;
    tk::Row& body = frame.body->add<tk::Row>();
    body.align(tk::Align::Start).space(theme.spacing.pad * 2.0f);
    body.grow();
    tk::Column& art = body.add<tk::Column>();
    art.grow(1.2f);
    art.add<ui::ImageView>(images_, shown->art, ui::OfferCard::artAspect);
    tk::Column& facts = body.add<tk::Column>();
    facts.grow();
    facts.space(theme.spacing.gap * 2.0f);
    facts.add<tk::Label>(offer.title, tk::TextRole::Title, true);
    facts.add<tk::Label>(controller_.lineOf(offer), tk::TextRole::Body)
        .colour(theme.colours.inkMuted);
    facts.add<tk::Label>(kindText(offer), tk::TextRole::Body).colour(theme.colours.inkMuted);
    if (shown->owned) {
        facts
            .add<tk::Label>("Owned: it is in your " + std::string{library::label(offer.store)} +
                                " library",
                            tk::TextRole::Body)
            .colour(theme.colours.accent);
    }
    facts.add<tk::Label>(claimText(offer), tk::TextRole::Caption, true)
        .colour(theme.colours.inkMuted);
    tk::Row& buttons = facts.add<tk::Row>();
    buttons.add<tk::Button>(
        claimId, shown->owned ? "Open store page" : "Claim",
        [this] {
            controller_.claim(offer_);
        },
        tk::ButtonKind::Primary);
    buttons.add<tk::Spacer>();
    return std::move(frame.root);
}

} // namespace opensu::app

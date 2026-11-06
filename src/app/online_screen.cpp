#include "online_screen.hpp"

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "online/invite_code.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

constexpr float cardDp = 640.0f;
/// A reply code with every address it can hold, and room for what a chat app adds.
constexpr std::size_t mostReplyBytes = 256;

} // namespace

std::unique_ptr<tk::Widget> OnlineScreen::build(const tk::Theme& theme) {
    const OnlinePlay::Page page = online_.shown();
    const tk::Spacing& spacing = theme.spacing;
    auto root = std::make_unique<tk::Centre>(cardDp);
    root->pad(tk::Insets::all(spacing.pad));
    tk::Card& card = root->add<tk::Card>();
    card.pad(tk::Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
    card.add<tk::Label>(page.title, tk::TextRole::Title, true);
    if (!page.note.empty()) {
        card.add<tk::Label>(page.note, tk::TextRole::Body, true);
    }
    if (!page.code.empty()) {
        card.add<tk::Label>(online::spaced(page.code), tk::TextRole::Body, true);
        card.add<tk::Button>(
            copyId, "Copy code",
            [this] {
                online_.copyCode();
            },
            tk::ButtonKind::Primary);
    }
    if (page.replyField) {
        card.add<tk::TextField>(fieldId, "Your friend's reply code", online_.replyDraft(),
                                [this](std::string text) {
                                    online_.editReply(std::move(text));
                                })
            .placeholder("Only if your friend gets one to send you")
            .limit(mostReplyBytes)
            .onSubmit([this] {
                online_.submitReply();
            });
    }
    card.add<tk::Button>(closeId, page.over ? "Close" : "Cancel", [this] {
        online_.close();
    });
    // A code that just appeared takes the focus, so A copies it and never cancels.
    if (page.code != codeSeen_) {
        codeSeen_ = page.code;
        if (!page.code.empty() && view().editing != fieldId) {
            view().focused = copyId;
        }
    }
    if (view().editing != fieldId) {
        card.add<tk::HintBar>();
    }
    return root;
}

void OnlineScreen::paintBackdrop(tk::Painter& painter, const gpu2d::Rect& viewport) const {
    painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

tk::Screen::BackReply OnlineScreen::back(tk::ActContext& /*context*/) {
    online_.close();
    return BackReply::Stay;
}

std::vector<tk::Hint> OnlineScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, online_.shown().over ? "Close" : "Cancel"}};
}

} // namespace opensu::app

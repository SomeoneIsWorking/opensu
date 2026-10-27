#include "downloads_screen.hpp"

#include <cstddef>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widgets.hpp"

#include "ui/page_frame.hpp"

namespace opensu::app {
namespace {

namespace tk = gpu2d::ui;

/// "Steam  ·  Downloading  ·  42%  ·  1.2 GB of 3 GB".
std::string lineOf(const DownloadsEntry& entry) {
    std::string line = entry.source + "  ·  " + entry.state;
    if (!entry.figures.empty()) {
        line += "  ·  " + entry.figures;
    }
    return line;
}

void addEntry(tk::Column& rows, const tk::Theme& theme, const DownloadsEntry& entry,
              DownloadsController& controller) {
    tk::Card& card = rows.add<tk::Card>();
    card.raised();
    card.pad(tk::Insets::symmetric(theme.spacing.pad, theme.spacing.pad * 0.8f));
    tk::Row& line = card.add<tk::Row>();
    line.align(tk::Align::Centre).space(theme.spacing.pad);
    tk::Column& text = line.add<tk::Column>();
    text.space(theme.spacing.gap);
    text.grow();
    tk::Label& title = text.add<tk::Label>(entry.title, tk::TextRole::Body);
    tk::Label& state = text.add<tk::Label>(lineOf(entry), tk::TextRole::Caption);
    if (entry.failed) {
        state.colour(theme.colours.danger);
    }
    if (entry.ended) {
        title.colour(theme.colours.inkMuted);
    }
    if (entry.fraction) {
        text.add<tk::Progress>(static_cast<float>(*entry.fraction));
    }
    for (std::size_t i = 0; i < entry.actions.size(); ++i) {
        line.add<tk::Button>(downloadButtonId(entry.key, i), entry.actions[i],
                             [&controller, key = entry.key, i] {
                                 controller.press(key, i);
                             });
    }
}

} // namespace

std::string downloadButtonId(const std::string& key, std::size_t action) {
    return "download/" + key + "/" + std::to_string(action);
}

std::unique_ptr<tk::Widget> DownloadsScreen::build(const tk::Theme& theme) {
    ui::PageFrame<tk::Column> frame = ui::pageFrame<tk::Column>(theme, chrome_);
    const std::vector<DownloadsEntry>& entries = controller_.entries();
    if (entries.empty()) {
        tk::Card& pane = frame.body->add<tk::Card>();
        pane.grow();
        tk::Centre& middle = pane.add<tk::Centre>(theme.spacing.dialogWidth);
        middle.grow();
        middle.add<tk::Label>("No downloads", tk::TextRole::Heading).colour(theme.colours.inkMuted);
        return std::move(frame.root);
    }
    tk::List& list = ui::rowsPane(*frame.body, theme, "downloads");
    for (const DownloadsEntry& entry : entries) {
        addEntry(list.rows(), theme, entry, controller_);
    }
    return std::move(frame.root);
}

tk::Screen::BackReply DownloadsScreen::back(tk::ActContext& /*context*/) {
    controller_.close();
    return BackReply::Stay;
}

bool DownloadsScreen::act(tk::Action action, tk::ActContext& /*context*/) {
    if (action != tk::Action::Menu) {
        return false;
    }
    controller_.close();
    return true;
}

std::vector<tk::Hint> DownloadsScreen::hints() const {
    return {tk::Hint{{tk::Action::Back}, "Back"}};
}

} // namespace opensu::app

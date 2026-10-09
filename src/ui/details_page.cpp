#include "details_page.hpp"

#include <algorithm>
#include <ctime>
#include <string>

#include "library/rom_systems.hpp"

namespace opensu::ui {
namespace {

constexpr float marginDp = 40.0f;
constexpr float gutterDp = 36.0f;
constexpr float buttonWidthDp = 280.0f;
constexpr float buttonHeightDp = 40.0f;
constexpr float buttonGapDp = 6.0f;
constexpr float radiusDp = 14.0f;
constexpr float coverAspect = 2.0f / 3.0f;
constexpr long long monthSeconds = 86400LL * 31;

std::string plural(long long count, const char* unit) {
    return std::to_string(count) + " " + unit + (count == 1 ? "" : "s") + " ago";
}

std::string badgeOf(const library::Game& game) {
    if (game.source == library::Source::Rom) {
        if (const library::roms::RomSystem* system = library::roms::systemByKey(game.sourceId)) {
            return std::string{system->label};
        }
    }
    return std::string{library::label(game.source)};
}

std::string playtimeText(int minutes) {
    if (minutes >= 60) {
        return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
    }
    return std::to_string(minutes) + " min";
}

/// The ROM's emulator as a row: its name and whether it is installed, or what to install.
std::string emulatorText(const library::Game& game) {
    if (game.emulatorOptions.empty()) {
        return "none known for this system";
    }
    const auto chosen =
        std::ranges::find(game.emulatorOptions, game.emulator, &library::EmulatorOption::name);
    if (chosen == game.emulatorOptions.end()) {
        return game.unavailable.empty() ? "none installed" : game.unavailable;
    }
    return chosen->name + (chosen->installed ? " (installed)" : " (not installed)");
}

} // namespace

std::string lastPlayedText(const std::optional<std::chrono::system_clock::time_point>& at,
                           std::chrono::system_clock::time_point now) {
    if (!at) {
        return "Never played";
    }
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - *at).count();
    if (seconds < 60) {
        return "Just now";
    }
    if (seconds < 3600) {
        return plural(seconds / 60, "minute");
    }
    if (seconds < 86400) {
        return plural(seconds / 3600, "hour");
    }
    if (seconds < monthSeconds) {
        return plural(seconds / 86400, "day");
    }
    const std::time_t when = std::chrono::system_clock::to_time_t(*at);
    std::tm parts{};
    localtime_r(&when, &parts);
    std::string date(16, '\0');
    date.resize(std::strftime(date.data(), date.size(), "%Y-%m-%d", &parts));
    return date;
}

DetailsView detailsFor(const library::Game& game, bool hidden,
                       std::chrono::system_clock::time_point now) {
    DetailsView view;
    view.gameId = game.id;
    view.title = game.title;
    view.badge = badgeOf(game);
    const bool rom = game.source == library::Source::Rom;
    view.rows.push_back(
        DetailsRow{"Status", std::string{game.installed ? "Installed" : "Not installed"} +
                                 (hidden ? ", hidden" : "")});
    if (game.ownedIn.size() > 1) {
        std::string owned;
        for (const library::Source source : game.ownedIn) {
            owned += (owned.empty() ? "" : ", ") + std::string{library::label(source)};
        }
        view.rows.push_back(DetailsRow{"Owned in", owned});
    }
    view.rows.push_back(DetailsRow{"Last played", lastPlayedText(game.lastPlayed, now)});
    if (game.playtimeMinutes > 0) {
        view.rows.push_back(DetailsRow{"Play time", playtimeText(game.playtimeMinutes)});
    }
    if (rom) {
        view.rows.push_back(DetailsRow{"Emulator", emulatorText(game)});
    }

    view.buttons.push_back(
        DetailsButton{DetailsAction::Launch, game.installed ? "Play" : "Install", ""});
    if (rom && game.emulatorOptions.size() > 1) {
        view.buttons.push_back(DetailsButton{DetailsAction::Emulator, "Emulator", game.emulator});
    }
    view.buttons.push_back(
        DetailsButton{DetailsAction::Hidden, hidden ? "Show game" : "Hide game", ""});
    view.buttons.push_back(DetailsButton{DetailsAction::Back, "Back", ""});
    return view;
}

std::optional<std::size_t> DetailsLayout::buttonAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

DetailsLayout layoutDetails(float width, float height, float dp, float topInset, float bottomInset,
                            std::size_t buttons) {
    DetailsLayout layout;
    const float margin = marginDp * dp;
    const float top = topInset;
    const float room = std::max(height - topInset - bottomInset, 0.0f);
    const float artHeight = room;
    const float artWidth = std::min(artHeight * coverAspect, width * 0.4f);
    layout.art = Rect{margin, top, artWidth, artHeight};
    layout.radius = radiusDp * dp;
    const float left = layout.art.right() + gutterDp * dp;
    const float columnWidth = std::min(buttonWidthDp * dp, std::max(width - left - margin, 0.0f));
    const auto count = static_cast<float>(buttons);
    const float stack =
        count * buttonHeightDp * dp + std::max(count - 1.0f, 0.0f) * buttonGapDp * dp;
    float y = layout.art.bottom() - stack;
    for (std::size_t i = 0; i < buttons; ++i) {
        layout.buttons.push_back(Rect{left, y, columnWidth, buttonHeightDp * dp});
        y += (buttonHeightDp + buttonGapDp) * dp;
    }
    layout.text = Rect{left, top, std::max(width - left - margin, 0.0f),
                       std::max(layout.art.bottom() - stack - top - gutterDp * 0.5f * dp, 0.0f)};
    return layout;
}

void DetailsPage::open(DetailsView view) {
    view_ = std::move(view);
    focus_ = 0;
    open_ = true;
}

void DetailsPage::refresh(DetailsView view) {
    if (!open_) {
        return;
    }
    if (view.gameId != view_.gameId) {
        open_ = false;
        return;
    }
    const DetailsAction was = selected();
    view_ = std::move(view);
    const auto same = std::ranges::find(view_.buttons, was, &DetailsButton::action);
    focus_ = same == view_.buttons.end() ? std::min(focus_, view_.buttons.size() - 1)
                                         : static_cast<std::size_t>(same - view_.buttons.begin());
}

bool DetailsPage::move(int delta) noexcept {
    const auto last = static_cast<int>(view_.buttons.size()) - 1;
    const int next = std::clamp(static_cast<int>(focus_) + delta, 0, std::max(last, 0));
    return focusButton(static_cast<std::size_t>(next));
}

bool DetailsPage::focusButton(std::size_t index) noexcept {
    if (index >= view_.buttons.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    return true;
}

} // namespace opensu::ui

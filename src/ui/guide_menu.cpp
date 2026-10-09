#include "guide_menu.hpp"

#include <algorithm>
#include <utility>

namespace opensu::ui {

namespace {

constexpr float panelWidthDp = 300.0f;
constexpr float paddingDp = 24.0f;
constexpr float itemHeightDp = 52.0f;
constexpr float itemGapDp = 6.0f;

} // namespace

std::optional<std::size_t> GuideLayout::rowAt(float x, float y) const noexcept {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].contains(x, y)) {
            return i;
        }
    }
    return std::nullopt;
}

GuideLayout layoutGuide(const PanelFrame& frame, const PanelChrome& chrome,
                        std::size_t rows) noexcept {
    const auto [width, height, dp] = frame;
    const auto [titleBox, footer] = chrome;
    GuideLayout layout;
    const float panel = std::min(panelWidthDp * dp, width);
    layout.panel = Rect{0.0f, 0.0f, panel, height};
    layout.padding = paddingDp * dp;
    const float inset = layout.padding * 0.5f;
    const float top = layout.padding + titleBox + layout.padding;
    // The rows give up height together when the list would run into the hints.
    const float room = std::max(height - top - footer - layout.padding, 0.0f);
    const float step = itemHeightDp * dp + itemGapDp * dp;
    const float wanted = static_cast<float>(rows) * step;
    const float squeeze = wanted > room && wanted > 0.0f ? room / wanted : 1.0f;
    float y = top;
    for (std::size_t i = 0; i < rows; ++i) {
        layout.rows.push_back(Rect{inset, y, panel - 2.0f * inset, itemHeightDp * dp * squeeze});
        y += step * squeeze;
    }
    return layout;
}

void GuideMenu::open(GuideContext context) {
    context_ = std::move(context);
    power_ = false;
    open_ = true;
    build();
}

void GuideMenu::close() noexcept {
    open_ = false;
}

std::string GuideMenu::title() const {
    if (power_) {
        return "Power";
    }
    return context_.inGame && !context_.title.empty() ? context_.title : "openSU";
}

std::string GuideMenu::shown(std::size_t index) const {
    const std::string& label = entries_[index].label;
    return armed_ && index == focus_ ? label + "? Press A again" : label;
}

void GuideMenu::build() {
    entries_.clear();
    focus_ = 0;
    armed_ = false;
    const auto add = [this](GuideAction action, std::string label) {
        entries_.push_back(GuideEntry{action, std::move(label)});
    };
    if (power_) {
        add(GuideAction::Sleep, "Sleep");
        add(GuideAction::Restart, "Restart");
        add(GuideAction::ShutDown, "Shut down");
        add(GuideAction::QuitToDesktop, "Quit to desktop");
        return;
    }
    if (context_.inGame) {
        add(GuideAction::Resume, "Resume");
        add(GuideAction::CloseGame, "Close game");
    } else {
        add(GuideAction::Home, "Home");
        add(GuideAction::Library, "Library");
        for (const library::Source store : context_.stores) {
            entries_.push_back(
                GuideEntry{GuideAction::Store, std::string{library::label(store)}, store});
        }
    }
    add(GuideAction::Devices, "Devices");
    add(GuideAction::Settings, "Settings");
    add(GuideAction::Power, "Power");
}

bool GuideMenu::move(int delta) noexcept {
    const auto last = static_cast<long>(entries_.size()) - 1;
    const std::size_t next =
        static_cast<std::size_t>(std::clamp(static_cast<long>(focus_) + delta, 0L, last));
    const bool moved = next != focus_;
    focus_ = next;
    armed_ = false;
    return moved;
}

bool GuideMenu::focusEntry(std::size_t index) noexcept {
    if (index >= entries_.size() || index == focus_) {
        return false;
    }
    focus_ = index;
    armed_ = false;
    return true;
}

void GuideMenu::showPower() {
    power_ = true;
    build();
}

void GuideMenu::showMain() {
    power_ = false;
    build();
    // Back from the power list lands on Power, the last entry.
    focus_ = entries_.size() - 1;
}

void GuideMenu::arm() noexcept {
    armed_ = true;
}

const char* GuideMenu::backLabel() const noexcept {
    if (power_) {
        return "Back";
    }
    return context_.inGame ? "Resume" : "Close";
}

} // namespace opensu::ui

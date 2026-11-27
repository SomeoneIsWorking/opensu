#include "quick_layout.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace opensu::app {
namespace {

using settings::QuickCategory;
using settings::QuickLayout;
using settings::QuickPin;
using settings::QuickTab;

/// The name a category the player adds starts with.
constexpr const char* newCategoryName = "New category";

template <typename T> std::size_t stepped(std::vector<T>& list, std::size_t at, int step) {
    if (at >= list.size()) {
        return at;
    }
    const std::size_t to = step < 0 ? (at == 0 ? 0 : at - 1) : std::min(list.size() - 1, at + 1);
    std::swap(list[at], list[to]);
    return to;
}

} // namespace

std::string_view key(QuickIcon icon) noexcept {
    switch (icon) {
    case QuickIcon::Sliders:
        return "sliders";
    case QuickIcon::Sound:
        return "sound";
    case QuickIcon::Brightness:
        return "brightness";
    case QuickIcon::Power:
        return "power";
    case QuickIcon::Display:
        return "display";
    case QuickIcon::Gamepad:
        return "gamepad";
    case QuickIcon::Moon:
        return "moon";
    case QuickIcon::Star:
        return "star";
    case QuickIcon::Network:
        return "network";
    case QuickIcon::Play:
        return "play";
    }
    return "sliders";
}

QuickIcon quickIconOf(std::string_view name) noexcept {
    const auto found = std::ranges::find(allQuickIcons, name, [](QuickIcon icon) {
        return key(icon);
    });
    return found == allQuickIcons.end() ? QuickIcon::Sliders : *found;
}

ui::Icon iconOf(QuickIcon icon) noexcept {
    switch (icon) {
    case QuickIcon::Sliders:
        return ui::Icon::QuickSliders;
    case QuickIcon::Sound:
        return ui::Icon::QuickSound;
    case QuickIcon::Brightness:
        return ui::Icon::QuickBrightness;
    case QuickIcon::Power:
        return ui::Icon::QuickPower;
    case QuickIcon::Display:
        return ui::Icon::QuickDisplay;
    case QuickIcon::Gamepad:
        return ui::Icon::QuickGamepad;
    case QuickIcon::Moon:
        return ui::Icon::QuickMoon;
    case QuickIcon::Star:
        return ui::Icon::QuickStar;
    case QuickIcon::Network:
        return ui::Icon::QuickNetwork;
    case QuickIcon::Play:
        return ui::Icon::QuickPlay;
    }
    return ui::Icon::QuickSliders;
}

QuickLayout QuickLayoutEditor::defaults() {
    const auto tab = [](QuickIcon icon, std::vector<QuickCategory> categories) {
        return QuickTab{std::string{key(icon)}, std::move(categories), false};
    };
    return QuickLayout{{
        tab(QuickIcon::Sliders,
            {QuickCategory{
                 "Sound", {{"audio", "system.volume", "Volume"}, {"audio", "system.mute", "Mute"}}},
             QuickCategory{"Screen",
                           {{"appearance", "system.brightness", "Brightness"},
                            {"appearance", "system.nightLight", "Night light"},
                            {"appearance", "system.scale", "Interface size"},
                            {"appearance", "system.screen", "Screen"}}},
             QuickCategory{"Power", {{"power", "system.powerProfile", "Power profile"}}},
             QuickCategory{"Player", {{"profiles", "profiles.playing", "Playing as"}}}}),
        QuickTab{std::string{key(QuickIcon::Play)}, {}, true},
        tab(QuickIcon::Display,
            {QuickCategory{"Shelf",
                           {{"library", "sort", "Sort by"},
                            {"library", "source", "Show"},
                            {"library", "installedOnly", "Installed games only"},
                            {"library", "hiddenOnly", "Hidden games only"},
                            {"appearance", "layout", "Library layout"},
                            {"appearance", "iconSize", "Icon size"}}}}),
        tab(QuickIcon::Gamepad,
            {QuickCategory{"Stick lights",
                           {{"controls", "lights.colour", "Stick lights"},
                            {"controls", "lights.brightness", "Stick light brightness"}}},
             QuickCategory{"Sticks", {{"controls", "sticks.correction", "Stick correction"}}}}),
    }};
}

bool QuickLayoutEditor::pinnable(const ui::SettingsRow& row) {
    if (row.id.empty()) {
        return false;
    }
    switch (row.kind) {
    case ui::RowKind::Toggle:
    case ui::RowKind::Choice:
    case ui::RowKind::Slider:
        return true;
    case ui::RowKind::Action:
        return row.confirm.empty();
    default:
        return false;
    }
}

QuickLayout QuickLayoutEditor::layout() const {
    QuickLayout layout = preferences_.values().quickSettings.value_or(defaults());
    if (std::ranges::none_of(layout.tabs, &QuickTab::game)) {
        const auto at = layout.tabs.begin() +
                        static_cast<std::ptrdiff_t>(std::min<std::size_t>(1, layout.tabs.size()));
        layout.tabs.insert(at, QuickTab{std::string{key(QuickIcon::Play)}, {}, true});
    }
    return layout;
}

std::size_t QuickLayoutEditor::gameTab() const {
    const QuickLayout now = layout();
    return static_cast<std::size_t>(std::ranges::find_if(now.tabs, &QuickTab::game) -
                                    now.tabs.begin());
}

bool QuickLayoutEditor::pinned(const QuickPin& pin) const {
    const QuickLayout now = layout();
    return std::ranges::any_of(now.tabs, [&pin](const QuickTab& tab) {
        return std::ranges::any_of(tab.categories, [&pin](const QuickCategory& category) {
            return std::ranges::find(category.items, pin) != std::ranges::end(category.items);
        });
    });
}

bool QuickLayoutEditor::togglePin(const QuickPin& pin) {
    QuickLayout now = layout();
    bool was = false;
    for (QuickTab& tab : now.tabs) {
        for (QuickCategory& category : tab.categories) {
            was = std::erase(category.items, pin) > 0 || was;
        }
    }
    if (!was) {
        auto first = std::ranges::find_if(now.tabs, [](const QuickTab& tab) {
            return !tab.game;
        });
        if (first == now.tabs.end()) {
            first = now.tabs.insert(now.tabs.begin(),
                                    QuickTab{std::string{key(QuickIcon::Sliders)}, {}, false});
        }
        std::vector<QuickCategory>& categories = first->categories;
        if (categories.empty()) {
            categories.push_back(QuickCategory{newCategoryName, {}});
        }
        categories.back().items.push_back(pin);
    }
    keep(std::move(now));
    return !was;
}

void QuickLayoutEditor::addItem(std::size_t tab, std::size_t category, const QuickPin& pin) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size() || now.tabs[tab].game ||
        category >= now.tabs[tab].categories.size()) {
        return;
    }
    std::vector<QuickPin>& items = now.tabs[tab].categories[category].items;
    if (std::ranges::find(items, pin) != std::ranges::end(items)) {
        return;
    }
    items.push_back(pin);
    keep(std::move(now));
}

void QuickLayoutEditor::removeItem(ItemPlace place) {
    QuickLayout now = layout();
    if (place.tab >= now.tabs.size() || place.category >= now.tabs[place.tab].categories.size()) {
        return;
    }
    std::vector<QuickPin>& items = now.tabs[place.tab].categories[place.category].items;
    if (place.item >= items.size()) {
        return;
    }
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(place.item));
    keep(std::move(now));
}

ItemPlace QuickLayoutEditor::moveItem(ItemPlace place, int step) {
    QuickLayout now = layout();
    if (place.tab >= now.tabs.size()) {
        return place;
    }
    std::vector<QuickCategory>& categories = now.tabs[place.tab].categories;
    if (place.category >= categories.size() ||
        place.item >= categories[place.category].items.size()) {
        return place;
    }
    std::vector<QuickPin>& items = categories[place.category].items;
    const QuickPin pin = items[place.item];
    if (step < 0 && place.item > 0) {
        std::swap(items[place.item], items[place.item - 1]);
        --place.item;
    } else if (step > 0 && place.item + 1 < items.size()) {
        std::swap(items[place.item], items[place.item + 1]);
        ++place.item;
    } else if (step < 0 && place.category > 0) {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(place.item));
        --place.category;
        categories[place.category].items.push_back(pin);
        place.item = categories[place.category].items.size() - 1;
    } else if (step > 0 && place.category + 1 < categories.size()) {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(place.item));
        ++place.category;
        std::vector<QuickPin>& into = categories[place.category].items;
        into.insert(into.begin(), pin);
        place.item = 0;
    } else {
        return place;
    }
    keep(std::move(now));
    return place;
}

std::size_t QuickLayoutEditor::addCategory(std::size_t tab, std::string name) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size() || now.tabs[tab].game) {
        return 0;
    }
    std::vector<QuickCategory>& categories = now.tabs[tab].categories;
    categories.push_back(
        QuickCategory{name.empty() ? std::string{newCategoryName} : std::move(name), {}});
    const std::size_t at = categories.size() - 1;
    keep(std::move(now));
    return at;
}

void QuickLayoutEditor::removeCategory(std::size_t tab, std::size_t category) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size() || category >= now.tabs[tab].categories.size()) {
        return;
    }
    std::vector<QuickCategory>& categories = now.tabs[tab].categories;
    categories.erase(categories.begin() + static_cast<std::ptrdiff_t>(category));
    keep(std::move(now));
}

void QuickLayoutEditor::renameCategory(std::size_t tab, std::size_t category, std::string name) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size() || category >= now.tabs[tab].categories.size()) {
        return;
    }
    now.tabs[tab].categories[category].name = std::move(name);
    keep(std::move(now));
}

std::size_t QuickLayoutEditor::moveCategory(std::size_t tab, std::size_t category, int step) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size()) {
        return category;
    }
    const std::size_t at = stepped(now.tabs[tab].categories, category, step);
    keep(std::move(now));
    return at;
}

std::size_t QuickLayoutEditor::addTab(QuickIcon icon) {
    QuickLayout now = layout();
    now.tabs.push_back(
        QuickTab{std::string{key(icon)}, {QuickCategory{newCategoryName, {}}}, false});
    const std::size_t at = now.tabs.size() - 1;
    keep(std::move(now));
    return at;
}

bool QuickLayoutEditor::removeTab(std::size_t tab) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size() || now.tabs[tab].game) {
        return false;
    }
    now.tabs.erase(now.tabs.begin() + static_cast<std::ptrdiff_t>(tab));
    keep(std::move(now));
    return true;
}

void QuickLayoutEditor::setIcon(std::size_t tab, QuickIcon icon) {
    QuickLayout now = layout();
    if (tab >= now.tabs.size()) {
        return;
    }
    now.tabs[tab].icon = std::string{key(icon)};
    keep(std::move(now));
}

std::size_t QuickLayoutEditor::moveTab(std::size_t tab, int step) {
    QuickLayout now = layout();
    const std::size_t at = stepped(now.tabs, tab, step);
    keep(std::move(now));
    return at;
}

void QuickLayoutEditor::keep(QuickLayout layout) {
    preferences_.values().quickSettings = std::move(layout);
    preferences_.save();
}

} // namespace opensu::app

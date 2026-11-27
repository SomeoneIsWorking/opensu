// quick_layout — what the Quick settings panel holds (`settings::Settings::quickSettings`): tabs
// shown as icons, each with named categories of items, an item being any Settings row the panel
// can act on as Settings does. Unset is the default layout. Every change to it goes through
// `QuickLayoutEditor`, which saves it.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "preferences.hpp"
#include "settings/settings.hpp"
#include "ui/setting_row.hpp"
#include "ui/vector_icon.hpp"

namespace opensu::app {

/// The icons a tab can show.
enum class QuickIcon : std::uint8_t {
    Sliders,
    Sound,
    Brightness,
    Power,
    Display,
    Gamepad,
    Moon,
    Star,
    Network,
    Play,
};

inline constexpr std::array allQuickIcons{
    QuickIcon::Sliders, QuickIcon::Sound,   QuickIcon::Brightness, QuickIcon::Power,
    QuickIcon::Display, QuickIcon::Gamepad, QuickIcon::Moon,       QuickIcon::Star,
    QuickIcon::Network, QuickIcon::Play,
};

/// The icon's spelling in the settings file.
[[nodiscard]] std::string_view key(QuickIcon icon) noexcept;
/// The icon a tab spelled `name` shows; sliders for one this build does not know.
[[nodiscard]] QuickIcon quickIconOf(std::string_view name) noexcept;
[[nodiscard]] ui::Icon iconOf(QuickIcon icon) noexcept;

/// Where an item is: its tab, its category on the tab, and its place in the category.
struct ItemPlace {
    std::size_t tab{};
    std::size_t category{};
    std::size_t item{};

    bool operator==(const ItemPlace&) const = default;
};

class QuickLayoutEditor {
  public:
    explicit QuickLayoutEditor(Preferences& preferences) : preferences_{preferences} {
    }

    /// What the panel holds until the player changes it.
    [[nodiscard]] static settings::QuickLayout defaults();
    /// Whether the panel can hold `row`: a switch, a choice, a slider, or an action that asks
    /// nothing first.
    [[nodiscard]] static bool pinnable(const ui::SettingsRow& row);

    /// The layout, with its Game tab: second when a kept layout has none.
    [[nodiscard]] settings::QuickLayout layout() const;
    /// Where the Game tab is.
    [[nodiscard]] std::size_t gameTab() const;
    /// Whether `pin` is anywhere in the panel.
    [[nodiscard]] bool pinned(const settings::QuickPin& pin) const;
    /// From Settings: adds `pin` to the first tab's last category (the first but the Game tab), or
    /// takes it out of every category; whether it is in now.
    bool togglePin(const settings::QuickPin& pin);

    /// Adds `pin` at the end of the category, unless it is in it already. The Game tab holds none.
    void addItem(std::size_t tab, std::size_t category, const settings::QuickPin& pin);
    void removeItem(ItemPlace place);
    /// Moves the item one place up (`step` < 0) or down, into the next category at a category's
    /// edge; where it is now.
    ItemPlace moveItem(ItemPlace place, int step);

    /// Adds a category at the end of the tab, not the Game tab; its index.
    std::size_t addCategory(std::size_t tab, std::string name);
    void removeCategory(std::size_t tab, std::size_t category);
    void renameCategory(std::size_t tab, std::size_t category, std::string name);
    /// Moves the category one place up or down; its index now.
    std::size_t moveCategory(std::size_t tab, std::size_t category, int step);

    /// Adds a tab at the end with one empty category; its index.
    std::size_t addTab(QuickIcon icon);
    /// Removes the tab; whether it did, the Game tab staying.
    bool removeTab(std::size_t tab);
    void setIcon(std::size_t tab, QuickIcon icon);
    /// Moves the tab one place left or right; its index now.
    std::size_t moveTab(std::size_t tab, int step);

    /// Keeps `layout` as it is: a grab put back.
    void keep(settings::QuickLayout layout);

  private:
    Preferences& preferences_;
};

} // namespace opensu::app

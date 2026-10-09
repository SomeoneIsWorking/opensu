// pointer_target — what the pointer is on, as the shell's own elements name it.
#pragma once

#include <cstddef>
#include <variant>

#include "gamepad/event.hpp"
#include "library/sections.hpp"

namespace opensu::ui {

/// A dock item.
struct OnDock {
    library::Section section;
    bool operator==(const OnDock&) const = default;
};

/// A home grid slot or a rail tile, by index.
struct OnTile {
    std::size_t index;
    bool operator==(const OnTile&) const = default;
};

/// A page arrow or a page dot, by the page it turns to.
struct OnPage {
    int page;
    bool operator==(const OnPage&) const = default;
};

/// A card of the Library layout picker.
struct OnLayoutCard {
    library::LibraryMode mode;
    bool operator==(const OnLayoutCard&) const = default;
};

/// A row of the Guide menu.
struct OnMenuItem {
    std::size_t index;
    bool operator==(const OnMenuItem&) const = default;
};

/// A hint on the launch panel, which presses the button it names.
struct OnPanelButton {
    gamepad::Button button;
    bool operator==(const OnPanelButton&) const = default;
};

/// Nothing, or the element the pointer is on.
using PointerTarget =
    std::variant<std::monostate, OnDock, OnTile, OnPage, OnLayoutCard, OnMenuItem, OnPanelButton>;

} // namespace opensu::ui

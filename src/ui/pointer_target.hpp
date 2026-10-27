// pointer_target — what the pointer is on, as the shell's own elements name it.
#pragma once

#include <cstddef>
#include <variant>

#include "library/game.hpp"
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

/// A key of the on-screen keyboard, by its index in `KeyboardLayout::keys`.
struct OnKeyboardKey {
    std::size_t index;
    bool operator==(const OnKeyboardKey&) const = default;
};

/// The interface toolkit's screens, which take the pointer themselves.
struct OnScreens {
    bool operator==(const OnScreens&) const = default;
};

/// A launcher's badge in the top bar; `downloading` when it shows a download's ring.
struct OnLauncher {
    library::Source source;
    bool downloading{false};
    bool operator==(const OnLauncher&) const = default;
};

/// A level of the breadcrumb trail, by its place in it.
struct OnCrumb {
    std::size_t index;
    bool operator==(const OnCrumb&) const = default;
};

/// Nothing, or the element the pointer is on.
using PointerTarget = std::variant<std::monostate, OnDock, OnTile, OnPage, OnKeyboardKey,
                                   OnLauncher, OnCrumb, OnScreens>;

} // namespace opensu::ui

// touch_controls — what an on-screen layout is made of, whichever controls it shows: the pieces it
// draws and the controls a finger takes. `TouchLayer` runs any such plan; `pad_layout` and
// `arcade_layout` make them.
#pragma once

#include <optional>
#include <vector>

#include "gamepad/event.hpp"
#include "touch/touch_surface.hpp"

namespace opensu::touch {

/// An image rasterised at the size it is drawn, RGBA, unpremultiplied.
struct Art {
    std::vector<unsigned char> rgba;
    int width{0};
    int height{0};
};

/// One image of the layout, stronger while a finger holds any of `lit`.
struct Piece {
    Box box;
    Art art;
    /// Drawn while held, when it has one.
    Art down{};
    std::vector<gamepad::Button> lit;
    /// How strongly it is drawn with no finger on it.
    float idle{0.65F};
    /// How far it moves towards the directions held.
    float lean{0.0F};
};

/// A direction control: the buttons held follow the finger's angle from its centre.
struct Stick {
    float x{0.0F};
    float y{0.0F};
    /// Nearer the centre than this, no direction is held.
    float deadZone{0.0F};
    /// 8, 4 or 2.
    unsigned ways{8};
    /// A 2-way stick moves up and down rather than left and right.
    bool vertical{false};
};

/// A button a finger can land on, and the point it is nearest to.
struct Target {
    float x{0.0F};
    float y{0.0F};
    gamepad::Button button{gamepad::Button::A};
};

/// Where a finger takes a control and what it then holds: `stick`'s directions, else the nearest
/// of `targets`, so a thumb slides among them.
struct Control {
    Box hit;
    /// `hit` is the circle in that box rather than the box.
    bool round{false};
    std::optional<Stick> stick;
    std::vector<Target> targets;
};

/// A whole layout. The first control whose `hit` holds a finger takes it.
struct ControlPlan {
    std::vector<Piece> pieces;
    std::vector<Control> controls;
    /// How much of the target's bottom the controls keep from the picture, in pixels.
    float band{0.0F};
};

/// The most of an upright target the controls take; beyond it they shrink.
inline constexpr float uprightShare = 0.5F;

} // namespace opensu::touch

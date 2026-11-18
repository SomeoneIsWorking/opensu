// console_layout — the on-screen controls of every layout but an arcade machine's panel, each a
// console's own buttons in its own letters and places (or a genre's: Racing, Platformer): a left
// cluster (d-pad, stick or steering) and a right one (faces, pedals), each with a smaller second
// beside it where the console has one (the other stick, the d-pad, the C buttons), shoulder rows,
// the middle pills and a pause button that holds Guide. In dp inside the safe area, hit regions
// larger than what is drawn; beside each other on a wide screen, stacked under the picture on an
// upright one.
#pragma once

#include "settings/video_settings.hpp"
#include "touch/label_face.hpp"
#include "touch/touch_art.hpp"
#include "touch/touch_controls.hpp"

namespace opensu::touch {

/// What to lay out: `layout` (never Auto), the console's own `console` that Racing and Platformer
/// take their buttons from, and whether the game reads analog sticks (a PlayStation game with
/// DualShock; a Nintendo 64 or gamepad game always does).
struct LayoutWish {
    settings::TouchLayout layout{settings::TouchLayout::Pad};
    settings::TouchLayout console{settings::TouchLayout::Pad};
    bool analog{false};

    bool operator==(const LayoutWish&) const = default;
};

/// `wish` on `surface`, with Rotate screen beside pause when `rotate`.
[[nodiscard]] ControlPlan consoleLayout(const WindowSurface& surface, const LayoutWish& wish,
                                        const ArtSource& art, LabelFace& labels, bool rotate);

} // namespace opensu::touch

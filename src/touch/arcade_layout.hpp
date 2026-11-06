// arcade_layout — an arcade game's own control panel: an arcade stick on the left, round buttons in
// the cabinet's rows on the right, each named for what it does, Coin and Start as small controls of
// their own and a pause button. Controls sit on the black beside the picture wherever they fit and
// keep to the screen's edges where they do not.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "touch/label_face.hpp"
#include "touch/touch_art.hpp"
#include "touch/touch_controls.hpp"

namespace opensu::touch {

/// A button of the machine and what it does.
enum class Limb : std::uint8_t {
    None,
    Punch,
    Kick,
};

struct ArcadeButton {
    gamepad::Button button{gamepad::Button::A};
    /// With a limb its strength, L, M, H or nothing, shown beside the limb's drawing.
    std::string label;
    Limb limb{Limb::None};

    bool operator==(const ArcadeButton&) const = default;
};

/// A machine's panel.
struct ArcadePanel {
    /// The stick's directions: 8, 4 or 2; 0 for a machine with no stick.
    unsigned ways{8};
    /// A 2-way stick moves up and down rather than left and right.
    bool vertical{false};
    std::vector<ArcadeButton> buttons;

    bool operator==(const ArcadePanel&) const = default;
};

/// `panel` on `surface` around the frame drawn at `picture`.
[[nodiscard]] ControlPlan arcadeLayout(const WindowSurface& surface, const Box& picture,
                                       const ArcadePanel& panel, const ArtSource& art,
                                       LabelFace& labels);

} // namespace opensu::touch

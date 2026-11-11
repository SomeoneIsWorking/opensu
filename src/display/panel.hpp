// panel — the glass of the screen a DRM connector drives, from its EDID.
#pragma once

#include <cstdint>
#include <span>

#include "recommended_scale.hpp"

namespace opensu::display {

/// The glass an EDID base block states, as KWin reads it: the first detailed timing's size in mm
/// when its shape is its mode's within 0.1, else the screen size in cm. Unknown for a short or
/// sizeless block.
[[nodiscard]] Panel panelOf(std::span<const std::uint8_t> edid, bool builtIn) noexcept;

/// The panel of the connected DRM connector, a built-in one before any other, which is the one
/// Gamescope shows on. Unknown with none connected, without an EDID, and off Linux.
[[nodiscard]] Panel connectedPanel();

} // namespace opensu::display

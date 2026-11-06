// panel_text — an arcade panel as text: how a runner tells its shell, and a host its guest, the
// machine's controls. The first line is the stick, `<ways> <vertical>`; each further line a button,
// `<button name> <limb> <label>`.
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "arcade_layout.hpp"

namespace opensu::touch {

[[nodiscard]] std::string panelText(const ArcadePanel& panel);
/// The panel `text` writes; nothing when it is malformed.
[[nodiscard]] std::optional<ArcadePanel> panelFromText(std::string_view text);

} // namespace opensu::touch

// size_text — a size in bytes as the player reads it, the one rule every screen uses.
#pragma once

#include <cstdint>
#include <string>

namespace opensu::ui {

/// `bytes` in decimal steps, so each unit is what its name says, as Steam and the desktop's file
/// managers show sizes: "45.2 GB", "512 B".
[[nodiscard]] std::string sizeText(std::uint64_t bytes);

} // namespace opensu::ui

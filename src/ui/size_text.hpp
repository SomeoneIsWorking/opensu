// size_text — a size in bytes as the player reads it.
#pragma once

#include <cstdint>
#include <string>

namespace opensu::ui {

/// `bytes` in binary steps with the short unit names: "45.2 GB", "512 B".
[[nodiscard]] std::string sizeText(std::uint64_t bytes);

} // namespace opensu::ui

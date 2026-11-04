// save_formats — a save carried between emulators of one console that lay its file out
// differently: each format read into the core's save RAM (`SaveFormat::SaveRam`) and written back
// out of it, through its console's owner (`mgba_saves`, `ps1_cards`).
#pragma once

#include <optional>
#include <string>

#include "game_saves.hpp"

namespace opensu::library::saves {

/// Whether a save in `from` must be converted to be one in `to`.
[[nodiscard]] bool reformatted(SaveFormat from, SaveFormat to) noexcept;

/// `bytes`, a save in `from`, as a save in `to`. Nothing, with why in `error`, when they are not a
/// save of `from` or `to` cannot hold them.
[[nodiscard]] std::optional<std::string> converted(SaveFormat from, SaveFormat to,
                                                   std::string bytes, std::string& error);

} // namespace opensu::library::saves

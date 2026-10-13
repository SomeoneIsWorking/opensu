// dump_info — what a console game folder says about itself, read before any name or box art
// lookup: a PS4 dump's sce_sys/param.sfo title and sce_sys/icon0.png.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "rom_systems.hpp"

namespace opensu::library::roms {

struct DumpInfo {
    /// The title the game gives itself, without trademark signs; empty when it gives none.
    std::string title;
    /// The game's own icon, or empty when it has none.
    std::filesystem::path icon;
};

/// The dump's own title and icon, for a game folder of a system that carries them.
[[nodiscard]] std::optional<DumpInfo> readDumpInfo(const RomSystem& system,
                                                   const std::filesystem::path& entry);

} // namespace opensu::library::roms

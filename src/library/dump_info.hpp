// dump_info — what a console game folder says about itself, read before any name or box art
// lookup: a PS4 dump's sce_sys/param.sfo title and sce_sys/icon0.png; a Switch file's title id; a
// Wii disc's game id from its header, in a plain image, an RVZ or WIA, or a WBFS.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "game.hpp"
#include "rom_systems.hpp"

namespace opensu::library::roms {

struct DumpInfo {
    /// The title the game gives itself, without trademark signs; empty when it gives none.
    std::string title;
    /// The game's own icon, or empty when it has none.
    std::filesystem::path icon;
    /// The id the game gives itself: a Switch game's title id as 16 upper-case hex digits, a Wii
    /// disc's six-character game id ("SF8E01"); empty when it gives none.
    std::string titleId;
    /// What a Switch game's control.nacp says of its saves; nothing for another game.
    std::optional<SwitchSaveData> saveData;
};

/// The dump's own title and icon, for a game folder of a system that carries them; a Wii disc's
/// game id, for its file.
[[nodiscard]] std::optional<DumpInfo> readDumpInfo(const RomSystem& system,
                                                   const std::filesystem::path& entry);

} // namespace opensu::library::roms

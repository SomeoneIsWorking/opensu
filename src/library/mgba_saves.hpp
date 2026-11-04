// mgba_saves — mGBA's saves of a Game Boy, Game Boy Color or Game Boy Advance ROM: where the
// standalone mGBA keeps `<rom stem>.sav` (beside the ROM, or in the folder its `config.ini`'s
// `savegamePath` names, a relative one under its settings folder; `mDirectorySetMapOptions`), and
// its `.sav` to and from the core's save RAM. mGBA appends a clock's state to a cartridge's save
// RAM (GBA's `GBASavedataRTCBuffer`, 16 bytes; GB's MBC3 `GBMBCRTCSaveBuffer`, 48 bytes, or 44 from
// older builds), and reads the save RAM as the file's size with the low 8 bits cleared (`size &
// ~0xFF`, `GBASavedataRTCRead`), so the save RAM is that and the rest is the clock's.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace opensu::library::saves {

/// Where the standalone mGBA keeps `rom`'s save, its settings in `config` (`~/.config/mgba`).
[[nodiscard]] std::filesystem::path mgbaSaveFile(const std::filesystem::path& config,
                                                 const std::filesystem::path& rom);

/// The core's save RAM in mGBA's `.sav`: the clock's footer dropped. Nothing, with `error`, for a
/// file too short to hold any.
[[nodiscard]] std::optional<std::string> saveRamOfMgba(std::string sav, std::string& error);

/// The core's save RAM as mGBA's `.sav`: the same bytes, with no clock, so mGBA starts the clock
/// from the host's. Nothing, with `error`, when its size is not one mGBA reads as save RAM.
[[nodiscard]] std::optional<std::string> mgbaOfSaveRam(std::string ram, std::string& error);

} // namespace opensu::library::saves

// switch_saves — a Switch emulator's saves of one title as its own files say. yuzu's line (Eden,
// Citron, sudachi) by the NAND path, `<nand>/user/save/0000000000000000/<user>/<title id>`, its
// profiles in `profiles.dat` and the current one in `qt-config.ini`; Ryujinx's line (Ryubing) by
// each save folder's own attribute in `ExtraData0`, its profiles in `system/Profiles.json`.
#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "game_saves.hpp"

namespace opensu::library::saves {

/// The account and device saves of `titleId` under yuzu's NAND folder `nand`, unmeasured; the
/// current profile is the one `<config>/qt-config.ini` picks.
[[nodiscard]] std::vector<SaveSlot> yuzuSlots(const std::filesystem::path& nand,
                                              const std::filesystem::path& config,
                                              std::string_view titleId);

/// The account and device saves of `titleId` under Ryujinx's `bis` folder, unmeasured: every
/// folder of `user/save`, or of a title's folder in it, whose `ExtraData0` names the title; the
/// current profile is the open one of `<config>/system/Profiles.json`.
[[nodiscard]] std::vector<SaveSlot> ryujinxSlots(const std::filesystem::path& bis,
                                                 const std::filesystem::path& config,
                                                 std::string_view titleId);

} // namespace opensu::library::saves

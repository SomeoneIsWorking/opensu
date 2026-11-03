// switch_saves — a Switch emulator's saves of one title as its own files say, and a save it does
// not have yet made as it would make it. yuzu's line (Eden, Citron, sudachi) by the NAND path,
// `<nand>/user/save/0000000000000000/<user>/<title id>` or Eden's newer
// `user/save/account/<UUID>/<title id>/0`, its profiles in `profiles.dat` and the current one in
// `qt-config.ini`; Ryujinx's line by each save folder's own attribute in `ExtraData0`, its
// profiles in `system/Profiles.json`. Ryubing names a save folder by the save id its index
// (`imkvdb.arc`) gives it; Kenji-NX by game and profile, `<title id>/<user>`, read back from the
// folders themselves.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
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

/// A slot an emulator does not have yet, as it would make it.
struct NewSlot {
    SaveSlot slot;
    /// What making it changes besides the slot, backed up with it: Ryubing's save index.
    std::vector<std::filesystem::path> touched;
};

/// Where `home`'s emulator would make its `kind` save of the title: the current profile's account
/// save, or the device save; after `earlier`, the slots planned before it for the same copy.
/// Nothing, with why in `error`, when it cannot be made here.
[[nodiscard]] std::optional<NewSlot> plannedSlot(const SaveHome& home, SlotKind kind,
                                                 const std::vector<NewSlot>& earlier,
                                                 std::string& error);

/// Makes `planned` as the emulator would: its folders, and in Ryujinx's line its `ExtraData0` and
/// `ExtraData1` and, in Ryubing's layout, the save's index entry. Throws
/// `std::filesystem::filesystem_error`.
void makeSlot(const SaveHome& home, const NewSlot& planned);

} // namespace opensu::library::saves

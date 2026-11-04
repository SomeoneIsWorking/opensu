// save_bundle — a game's saves as one openSU sends them to another: the files of each slot a copy
// carries (`library::saves::copiedSlots`), by slot kind and path inside the slot, with their bytes
// and modification times; its content hash, which says whether two openSUs hold the same saves;
// and the bundle written out under a staging folder as saves a copy can take
// (`library::saves::SaveCopier`). Paths from another openSU are checked: relative, no `..`.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "library/game_saves.hpp"

namespace opensu::lan {

/// The largest bundle sent or taken.
inline constexpr std::size_t largestBundle = std::size_t{512} * 1024 * 1024;

struct BundleFile {
    library::saves::SlotKind kind{library::saves::SlotKind::Memory};
    /// Inside the slot, `/`-separated; empty when the slot is one file.
    std::string path;
    std::string bytes;
    std::chrono::system_clock::time_point modified;
};

struct SaveBundle {
    std::vector<BundleFile> files;

    /// SHA-256, as hex, over each file's kind, path and bytes in a fixed order; times do not count.
    [[nodiscard]] std::string hash() const;
    [[nodiscard]] std::uintmax_t bytes() const;
    /// The newest file's time; nothing when it holds none.
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> modified() const;
};

/// The files of `saves`' copied slots, read whole. Nothing, with `error`, when one cannot be read
/// or they come to more than `largestBundle`.
[[nodiscard]] std::optional<SaveBundle> bundleOf(const library::saves::EmulatorSaves& saves,
                                                 std::string& error);

/// The bundle as it crosses the wire: a JSON header naming each file, then the files' bytes.
[[nodiscard]] std::string write(const SaveBundle& bundle);
/// A bundle read back; nothing when `bytes` is not one or names a path outside its slot.
[[nodiscard]] std::optional<SaveBundle> readBundle(std::string_view bytes);

/// `bundle` written under `folder`, emptied first, as the saves `name` keeps: one slot per kind it
/// holds, for a copy to read from. Throws `std::filesystem::filesystem_error`.
[[nodiscard]] library::saves::EmulatorSaves
staged(const SaveBundle& bundle, const std::filesystem::path& folder, std::string name);

} // namespace opensu::lan

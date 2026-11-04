// save_copy — one emulator's saves of a game copied over another's, slot by slot for each kind
// the source holds files in (`copiedSlots`); a slot the target lacks is made as that emulator
// would make it (`plannedSlot`), one it keeps where nothing is yet is made there. The source can
// be another openSU's saves staged on disk (`lan::SaveSync`). The target's slots, and what making
// one changes, are copied whole into a backup first, `<backups>/<system>/<game>/<emulator>/<UTC
// time>/` with `backup.json` naming where each came from and what was made, and undo puts the
// newest backup back byte for byte, removes what the copy made, and removes the backup.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game_saves.hpp"

namespace opensu::library::saves {

/// One backup of an emulator's saves, made before a copy over them.
struct SaveBackup {
    std::filesystem::path folder;
    std::chrono::system_clock::time_point made;
    /// The emulator whose saves were copied over them.
    std::string from;
};

/// How far a copy or an undo is, in bytes written; read from any thread.
struct CopyProgress {
    std::atomic<std::uintmax_t> done{0};
    std::atomic<std::uintmax_t> total{0};
};

/// The slots of `from` a copy carries: for each kind, the one `slotFor` picks, when it holds files.
[[nodiscard]] std::vector<const SaveSlot*> copiedSlots(const EmulatorSaves& from);

class SaveCopier {
  public:
    /// Backups are kept under `backups` (`<state dir>/save-backups`).
    explicit SaveCopier(std::filesystem::path backups) : backups_{std::move(backups)} {
    }

    /// Copies `from`'s saves of `key` over `to`'s, `to`'s backed up first, counting into
    /// `progress`. Empty when done, else why not, with `to`'s saves as they were.
    [[nodiscard]] std::string copy(const SaveKey& key, const EmulatorSaves& from,
                                   const EmulatorSaves& to,
                                   std::chrono::system_clock::time_point now,
                                   CopyProgress& progress) const;

    /// The newest backup of `emulatorId`'s saves of `key`; nothing when there is none.
    [[nodiscard]] std::optional<SaveBackup> latest(const SaveKey& key,
                                                   std::string_view emulatorId) const;

    /// Puts the newest backup of `emulatorId`'s saves of `key` back, removes what its copy made,
    /// and removes it, counting into `progress`. Empty when done, else why not.
    [[nodiscard]] std::string undo(const SaveKey& key, std::string_view emulatorId,
                                   CopyProgress& progress) const;

  private:
    [[nodiscard]] std::filesystem::path folderOf(const SaveKey& key,
                                                 std::string_view emulatorId) const;

    std::filesystem::path backups_;
};

} // namespace opensu::library::saves

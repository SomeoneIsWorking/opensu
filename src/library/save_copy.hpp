// save_copy — one emulator's saves of a game copied over another's, slot by slot for each kind
// both keep (`EmulatorSaves::slotFor`). The target's slots are copied whole into a backup first,
// `<backups>/<system>/<game>/<emulator>/<UTC time>/` with `backup.json` naming where each came
// from, and undo puts the newest backup back byte for byte and removes it.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "game_saves.hpp"

namespace opensu::library::saves {

/// One backup of an emulator's saves, made before a copy over them.
struct SaveBackup {
    std::filesystem::path folder;
    std::chrono::system_clock::time_point made;
    /// The emulator whose saves were copied over them.
    std::string from;
};

class SaveCopier {
  public:
    /// Backups are kept under `backups` (`<state dir>/save-backups`).
    explicit SaveCopier(std::filesystem::path backups) : backups_{std::move(backups)} {
    }

    /// Copies `from`'s saves of `key` over `to`'s, `to`'s backed up first. Empty when done, else
    /// why not, with `to`'s saves as they were.
    [[nodiscard]] std::string copy(const SaveKey& key, const EmulatorSaves& from,
                                   const EmulatorSaves& to,
                                   std::chrono::system_clock::time_point now) const;

    /// The newest backup of `emulatorId`'s saves of `key`; nothing when there is none.
    [[nodiscard]] std::optional<SaveBackup> latest(const SaveKey& key,
                                                   std::string_view emulatorId) const;

    /// Puts the newest backup of `emulatorId`'s saves of `key` back and removes it. Empty when
    /// done, else why not.
    [[nodiscard]] std::string undo(const SaveKey& key, std::string_view emulatorId) const;

  private:
    [[nodiscard]] std::filesystem::path folderOf(const SaveKey& key,
                                                 std::string_view emulatorId) const;

    std::filesystem::path backups_;
};

} // namespace opensu::library::saves

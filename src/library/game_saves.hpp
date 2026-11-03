// game_saves — the one owner of "this game's saves": for a ROM, each installed emulator of its
// system that keeps saves, the save slots it keeps for the game, found by console and game
// identity (a Switch title id, the ROM's name for the built-in runner) rather than by path, each
// with its size and newest change, and how it would make a slot it does not have yet.
// Standalone emulators' save folders come from the catalog (`SaveStore`, its Flatpak's when it is
// installed as one), the built-in runner's files from its own path owner (`retro/save_paths`).
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "emulator_catalog.hpp"
#include "game.hpp"

namespace opensu::library::saves {

/// A game as its saves know it: the console and the game's own id there.
struct SaveKey {
    std::string system;
    /// A Switch title id; the ROM's name without its extension for the built-in runner.
    std::string game;

    bool operator==(const SaveKey&) const = default;
};

/// What a save slot holds.
enum class SlotKind : std::uint8_t {
    /// A Switch save of one user profile.
    Account,
    /// A Switch save the whole console shares.
    Device,
    /// The built-in runner's save RAM.
    Memory,
    /// The built-in runner's state slot.
    State,
};

struct SaveSlot {
    SlotKind kind{SlotKind::Account};
    /// The profile an account save belongs to, as the emulator spells it; empty otherwise.
    std::string user;
    /// Whether it is the save of the emulator's current profile.
    bool current{false};
    /// What a backup copies whole and undo puts back: a save folder, or the runner's file.
    std::filesystem::path root;
    /// Where the game's files are: read from the first, a copy written to each.
    std::vector<std::filesystem::path> content;
    std::uintmax_t bytes{0};
    /// The newest file's modification; nothing while the slot holds no file.
    std::optional<std::chrono::system_clock::time_point> modified;
};

/// Where a standalone emulator keeps a game's saves, which a slot it lacks is made in.
struct SaveHome {
    roms::SaveLayout layout{roms::SaveLayout::YuzuNand};
    /// Its NAND or `bis` folder, its Flatpak's when it is installed as one.
    std::filesystem::path folder;
    /// Its settings folder, where its current profile is chosen.
    std::filesystem::path config;
    std::string titleId;
    /// What the game's control.nacp says of its saves; nothing when it is not known.
    std::optional<SwitchSaveData> saveData;
};

/// One emulator's saves of the game.
struct EmulatorSaves {
    std::string emulatorId;
    std::string emulatorName;
    std::vector<SaveSlot> slots;
    /// Where a slot it lacks is made; nothing for the built-in runner.
    std::optional<SaveHome> home;
    /// What its running process's command line holds (`EmulatorOption::processHint`); empty for
    /// the built-in runner.
    std::string processHint;

    [[nodiscard]] std::uintmax_t bytes() const;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> modified() const;
    /// Whether a slot holds a file: only then is it a copy's source.
    [[nodiscard]] bool holdsFiles() const {
        return modified().has_value();
    }
    /// The slot of `kind` a copy reads or writes: the current profile's, else the only one; null
    /// when there is none or several and none is current.
    [[nodiscard]] const SaveSlot* slotFor(SlotKind kind) const;
};

/// The built-in runner's entry in `savesOf`, one for every core, which share its folder.
inline constexpr std::string_view runnerSavesId = "built-in";

/// What `game`'s saves are found by; nothing for a game that is no ROM, or a Switch file whose
/// title id could not be read.
[[nodiscard]] std::optional<SaveKey> keyOf(const Game& game);

/// The slots `home` keeps for its title, measured.
[[nodiscard]] std::vector<SaveSlot> storedSlots(const SaveHome& home);

/// Each installed emulator of `game` that keeps saves, in the game's emulator order, with the
/// slots it keeps of the game, none when it has none yet: standalone ones through their catalog
/// `SaveStore`, the built-in runner's `.srm` and state under `runnerSaves` (`Config::savesDir`),
/// listed only when it keeps one.
[[nodiscard]] std::vector<EmulatorSaves> savesOf(const Game& game,
                                                 const roms::EmulatorCatalog& catalog,
                                                 const std::filesystem::path& runnerSaves);

/// Whether `saves` can be copied between: one holds files and another can take them.
[[nodiscard]] bool copyable(const std::vector<EmulatorSaves>& saves);

} // namespace opensu::library::saves

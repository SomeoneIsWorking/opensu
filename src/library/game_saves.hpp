// game_saves — the one owner of "this game's saves": for a ROM, each installed emulator of its
// system that keeps saves, the save slots it keeps for the game, found by console and game
// identity (a Switch title id, the ROM's name for the built-in runner) and the player profile
// they are kept for rather than by path, each
// with its size and newest change, and how it would make a slot it does not have yet.
// Standalone emulators' save folders come from the catalog (`SaveStore`, its Flatpak's when it is
// installed as one), the built-in runner's files from its own path owner (`retro/save_paths`).
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"
#include "game.hpp"

namespace opensu::library::saves {

/// A game's saves as they are known: the console, the game's own id there, and whose they are.
struct SaveKey {
    std::string system;
    /// A Switch title id; the ROM's name without its extension for the built-in runner.
    std::string game;
    /// The player profile they are kept for (`profiles::Profile::id`).
    std::string profile;

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
    /// The resume state Save and close left, which goes with the save RAM it was taken with.
    Resume,
    /// The memory card a built-in core keeps of the game itself: LRPS2's `.ps2`.
    Card,
};

/// Every kind, in the order a copy takes them.
inline constexpr std::array slotKinds{SlotKind::Account, SlotKind::Device, SlotKind::Memory,
                                      SlotKind::State,   SlotKind::Resume, SlotKind::Card};

/// How a save RAM file is laid out, where emulators of one console differ (`save_formats`).
enum class SaveFormat : std::uint8_t {
    /// Copied as it is: a Switch save folder, the runner's state slot.
    AsIs,
    /// The core's save RAM alone, as the runner keeps it in its `.srm`; formats convert through
    /// it.
    SaveRam,
    /// mGBA's `.sav`: the save RAM, then a clock's footer for a cartridge with one.
    Mgba,
    /// DuckStation's memory card, `.mcd`.
    DuckStationCard,
};

struct SaveSlot {
    SlotKind kind{SlotKind::Account};
    /// The profile an account save belongs to, as the emulator spells it; empty otherwise.
    std::string user;
    /// Whether it is the account save of the emulator user the profile plays as
    /// (`SaveHome::seat`).
    bool current{false};
    /// What a backup copies whole and undo puts back: a save folder, or the runner's file.
    std::filesystem::path root;
    /// Where the game's files are: read from the first, a copy written to each.
    std::vector<std::filesystem::path> content;
    /// How its file is laid out; `AsIs` for a folder.
    SaveFormat format{SaveFormat::AsIs};
    std::uintmax_t bytes{0};
    /// The newest file's modification; nothing while the slot holds no file, or is not there yet.
    std::optional<std::chrono::system_clock::time_point> modified;
};

/// Where a standalone emulator keeps a game's saves, which a slot it lacks is made in.
struct SaveHome {
    roms::SaveLayout layout{roms::SaveLayout::YuzuNand};
    /// Its NAND or `bis` folder, its Flatpak's when it is installed as one.
    std::filesystem::path folder;
    /// Its settings folder, where its current profile is chosen.
    std::filesystem::path config;
    /// The profile's place in openSU's order, which picks the emulator user it plays as.
    std::size_t seat{0};
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

    // Of the profile's slots: its user's account save, and the slots that are no account save.
    [[nodiscard]] std::uintmax_t bytes() const;
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> modified() const;
    /// Whether a slot of the profile's holds a file: only then is it a copy's source.
    [[nodiscard]] bool holdsFiles() const {
        return modified().has_value();
    }
    /// The slot of `kind` a copy reads or writes: an account save the profile's user's, any other
    /// the only one; null when there is none, or several.
    [[nodiscard]] const SaveSlot* slotFor(SlotKind kind) const;
};

/// The built-in runner's entry in `savesOf`, one for every core, which share its folder.
inline constexpr std::string_view runnerSavesId = "built-in";

/// What `profile`'s saves of `game` are found by; nothing for a game that is no ROM, or a Switch
/// file whose title id could not be read.
[[nodiscard]] std::optional<SaveKey> keyOf(const Game& game, std::string_view profile);

/// The slots `home` keeps for its title, measured.
[[nodiscard]] std::vector<SaveSlot> storedSlots(const SaveHome& home);

/// Each installed emulator of `game` that keeps saves, in the game's emulator order, with the
/// slots it keeps of the game for `profile`, which sits `seat` in openSU's order
/// (`ProfileBook::seatOf`), none when it has none yet: standalone ones through
/// their catalog `SaveStore`, the built-in runner's `.srm` and state in the profile's folder under
/// `runnerSaves` (`Config::savesDir`), and LRPS2's card where LRPS2 is one of its cores, each
/// listed where the runner keeps it, holding a file or not.
[[nodiscard]] std::vector<EmulatorSaves> savesOf(const Game& game,
                                                 const roms::EmulatorCatalog& catalog,
                                                 const std::filesystem::path& runnerSaves,
                                                 std::string_view profile, std::size_t seat);

/// Among `saves` (`savesOf(game)`), those of the emulator `game` starts with (`Game::emulator`);
/// null when it keeps none there.
[[nodiscard]] const EmulatorSaves* launchedSaves(const std::vector<EmulatorSaves>& saves,
                                                 const Game& game,
                                                 const roms::EmulatorCatalog& catalog);

/// Where the built-in runner keeps the resume state Save and close leaves of ROM `game` for
/// `profile`, in the profile's folder under `runnerSaves`.
[[nodiscard]] std::filesystem::path runnerResumeFile(const std::filesystem::path& runnerSaves,
                                                     std::string_view profile, const Game& game);

/// Whether `saves` can be copied between: one holds files and another can take them.
[[nodiscard]] bool copyable(const std::vector<EmulatorSaves>& saves);

} // namespace opensu::library::saves

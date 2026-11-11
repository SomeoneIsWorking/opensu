#include "game_saves.hpp"

#include <algorithm>
#include <ranges>
#include <system_error>
#include <utility>

#include "clock/file_time.hpp"
#include "mgba_saves.hpp"
#include "ps1_cards.hpp"
#include "retro/lrps2_card.hpp"
#include "retro/save_paths.hpp"
#include "switch_saves.hpp"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;
using TimePoint = std::chrono::system_clock::time_point;

constexpr std::string_view switchSystem = "switch";
constexpr std::string_view runnerName = "Built-in";

void keepNewest(std::optional<TimePoint>& newest, fs::file_time_type written) {
    const TimePoint at{std::chrono::duration_cast<std::chrono::system_clock::duration>(
        clock::sinceEpoch(written))};
    if (!newest || at > *newest) {
        newest = at;
    }
}

/// The bytes and newest change of the files the slot's first content holds.
void measure(SaveSlot& slot) {
    if (slot.content.empty()) {
        return;
    }
    const fs::path& files = slot.content.front();
    std::error_code ec;
    if (fs::is_regular_file(files, ec)) {
        slot.bytes = fs::file_size(files, ec);
        keepNewest(slot.modified, fs::last_write_time(files, ec));
        return;
    }
    for (fs::recursive_directory_iterator it{files, ec}, end; it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        slot.bytes += it->file_size(ec);
        keepNewest(slot.modified, it->last_write_time(ec));
    }
}

fs::path runnerFolder(const fs::path& saves, std::string_view profile, const Game& game) {
    return retro::systemSaveFolder(retro::profileSaveFolder(saves, profile), game.sourceId);
}

/// Whether an installed built-in emulator of `game` runs LRPS2, which keeps its own card.
bool keepsLrps2Card(const Game& game, const roms::EmulatorCatalog& catalog) {
    return std::ranges::any_of(game.emulatorOptions, [&catalog](const EmulatorOption& option) {
        const roms::EmulatorEntry* entry = option.installed ? catalog.find(option.id) : nullptr;
        return entry != nullptr && entry->builtIn() &&
               retro::coreName(entry->core) == retro::lrps2Core;
    });
}

/// The runner's save RAM, state and resume state of `game` for `profile`, and LRPS2's card with
/// `card`, each where the runner keeps it whether it is there yet or not.
std::vector<SaveSlot> runnerSlots(const fs::path& saves, std::string_view profile, const Game& game,
                                  bool card) {
    const fs::path folder = runnerFolder(saves, profile, game);
    std::vector<std::pair<SlotKind, fs::path>> files{
        {SlotKind::Memory, retro::saveMemoryFile(folder, game.file)},
        {SlotKind::State, retro::saveStateFile(folder, game.file)},
        {SlotKind::Resume, retro::resumeStateFile(folder, game.file)}};
    if (card) {
        files.emplace_back(SlotKind::Card, retro::lrps2CardFile(folder, game.file));
    }
    std::vector<SaveSlot> slots;
    for (const auto& [kind, file] : files) {
        slots.push_back(
            SaveSlot{.kind = kind,
                     .root = file,
                     .content = {file},
                     .format = kind == SlotKind::Memory ? SaveFormat::SaveRam : SaveFormat::AsIs});
    }
    return slots;
}

/// The one save file a standalone emulator of a cartridge or disc console keeps of `game`, its
/// settings in `config`, where it is or will be; nothing for a layout of folders, or when it keeps
/// none per game.
std::optional<SaveSlot> fileSlot(roms::SaveLayout layout, const fs::path& folder,
                                 const fs::path& config, const Game& game) {
    std::optional<fs::path> file;
    SaveFormat format = SaveFormat::AsIs;
    switch (layout) {
    case roms::SaveLayout::Mgba:
        file = mgbaSaveFile(config, game.file);
        format = SaveFormat::Mgba;
        break;
    case roms::SaveLayout::DuckStationCards:
        file = duckStationCard(config, folder, game.title, game.file);
        format = SaveFormat::DuckStationCard;
        break;
    case roms::SaveLayout::YuzuNand:
    case roms::SaveLayout::RyujinxBis:
        break;
    }
    if (!file) {
        return std::nullopt;
    }
    return SaveSlot{.kind = SlotKind::Memory, .root = *file, .content = {*file}, .format = format};
}

/// Whether `slot` is the profile's: an account save only when it is its user's.
bool profiles(const SaveSlot& slot) {
    return slot.kind != SlotKind::Account || slot.current;
}

} // namespace

std::uintmax_t EmulatorSaves::bytes() const {
    std::uintmax_t total = 0;
    for (const SaveSlot& slot : slots | std::views::filter(profiles)) {
        total += slot.bytes;
    }
    return total;
}

std::optional<TimePoint> EmulatorSaves::modified() const {
    std::optional<TimePoint> newest;
    for (const SaveSlot& slot : slots | std::views::filter(profiles)) {
        if (slot.modified && (!newest || *slot.modified > *newest)) {
            newest = slot.modified;
        }
    }
    return newest;
}

const SaveSlot* EmulatorSaves::slotFor(SlotKind kind) const {
    const SaveSlot* only = nullptr;
    std::size_t count = 0;
    for (const SaveSlot& slot : slots | std::views::filter(profiles)) {
        if (slot.kind != kind) {
            continue;
        }
        if (slot.current) {
            return &slot;
        }
        only = &slot;
        ++count;
    }
    return count == 1 ? only : nullptr;
}

std::optional<SaveKey> keyOf(const Game& game, std::string_view profile) {
    if (game.source != Source::Rom || game.file.empty()) {
        return std::nullopt;
    }
    if (game.sourceId == switchSystem) {
        if (game.titleId.empty()) {
            return std::nullopt;
        }
        return SaveKey{
            .system = game.sourceId, .game = game.titleId, .profile = std::string{profile}};
    }
    return SaveKey{.system = game.sourceId,
                   .game = game.file.stem().string(),
                   .profile = std::string{profile}};
}

std::vector<SaveSlot> storedSlots(const SaveHome& home) {
    std::vector<SaveSlot> slots =
        home.layout == roms::SaveLayout::YuzuNand
            ? yuzuSlots(home.folder, home.config, home.seat, home.titleId)
            : ryujinxSlots(home.folder, home.config, home.seat, home.titleId);
    std::ranges::for_each(slots, measure);
    return slots;
}

const EmulatorSaves* launchedSaves(const std::vector<EmulatorSaves>& saves, const Game& game,
                                   const roms::EmulatorCatalog& catalog) {
    const auto option =
        std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    const roms::EmulatorEntry* entry =
        option != game.emulatorOptions.end() ? catalog.find(option->id) : nullptr;
    if (entry == nullptr) {
        return nullptr;
    }
    const std::string id = entry->builtIn() ? std::string{runnerSavesId} : entry->id;
    const auto found = std::ranges::find(saves, id, &EmulatorSaves::emulatorId);
    return found == saves.end() ? nullptr : &*found;
}

fs::path runnerResumeFile(const fs::path& runnerSaves, std::string_view profile, const Game& game) {
    return retro::resumeStateFile(runnerFolder(runnerSaves, profile, game), game.file);
}

bool copyable(const std::vector<EmulatorSaves>& saves) {
    return saves.size() >= 2 && std::ranges::any_of(saves, &EmulatorSaves::holdsFiles);
}

std::vector<EmulatorSaves> savesOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                   const fs::path& runnerSaves, std::string_view profile,
                                   std::size_t seat) {
    const std::optional<SaveKey> key = keyOf(game, profile);
    if (!key) {
        return {};
    }
    std::vector<EmulatorSaves> found;
    bool runnerListed = false;
    for (const EmulatorOption& option : game.emulatorOptions) {
        const roms::EmulatorEntry* entry = option.installed ? catalog.find(option.id) : nullptr;
        if (entry == nullptr) {
            continue;
        }
        EmulatorSaves saves{.emulatorId = entry->id,
                            .emulatorName = entry->name,
                            .slots = {},
                            .home = std::nullopt,
                            .processHint = {}};
        if (entry->builtIn()) {
            if (runnerListed) {
                continue;
            }
            runnerListed = true;
            saves = EmulatorSaves{.emulatorId = std::string{runnerSavesId},
                                  .emulatorName = std::string{runnerName},
                                  .slots = runnerSlots(runnerSaves, profile, game,
                                                       keepsLrps2Card(game, catalog)),
                                  .home = std::nullopt,
                                  .processHint = {}};
            std::ranges::for_each(saves.slots, measure);
            found.push_back(std::move(saves));
            continue;
        }
        if (!entry->saves) {
            continue;
        }
        const bool flatpak = option.flatpak && !entry->saves->flatpakFolder.empty();
        const fs::path& config = entry->configOf(option.flatpak);
        saves.processHint = option.processHint;
        const fs::path& folder = flatpak ? entry->saves->flatpakFolder : entry->saves->folder;
        if (const std::optional<SaveSlot> file =
                fileSlot(entry->saves->layout, folder, config, game)) {
            saves.slots = {*file};
            std::ranges::for_each(saves.slots, measure);
            found.push_back(std::move(saves));
            continue;
        }
        if (key->system != switchSystem) {
            continue;
        }
        saves.home = SaveHome{.layout = entry->saves->layout,
                              .folder = folder,
                              .config = config,
                              .seat = seat,
                              .titleId = key->game,
                              .saveData = game.switchSaveData};
        saves.slots = storedSlots(*saves.home);
        found.push_back(std::move(saves));
    }
    return found;
}

} // namespace opensu::library::saves

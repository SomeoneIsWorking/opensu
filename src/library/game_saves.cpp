#include "game_saves.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "clock/file_time.hpp"
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

/// The runner's save RAM and state of `rom`, those that are there.
std::vector<SaveSlot> runnerSlots(const fs::path& saves, const Game& game) {
    const fs::path folder = retro::systemSaveFolder(saves, game.sourceId);
    std::vector<SaveSlot> slots;
    for (const auto& [kind, file] :
         {std::pair{SlotKind::Memory, retro::saveMemoryFile(folder, game.file)},
          std::pair{SlotKind::State, retro::saveStateFile(folder, game.file)}}) {
        std::error_code ec;
        if (fs::is_regular_file(file, ec)) {
            slots.push_back(SaveSlot{.kind = kind, .root = file, .content = {file}});
        }
    }
    return slots;
}

} // namespace

std::uintmax_t EmulatorSaves::bytes() const {
    std::uintmax_t total = 0;
    for (const SaveSlot& slot : slots) {
        total += slot.bytes;
    }
    return total;
}

std::optional<TimePoint> EmulatorSaves::modified() const {
    std::optional<TimePoint> newest;
    for (const SaveSlot& slot : slots) {
        if (slot.modified && (!newest || *slot.modified > *newest)) {
            newest = slot.modified;
        }
    }
    return newest;
}

const SaveSlot* EmulatorSaves::slotFor(SlotKind kind) const {
    const SaveSlot* only = nullptr;
    std::size_t count = 0;
    for (const SaveSlot& slot : slots) {
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

std::optional<SaveKey> keyOf(const Game& game) {
    if (game.source != Source::Rom || game.file.empty()) {
        return std::nullopt;
    }
    if (game.sourceId == switchSystem) {
        if (game.titleId.empty()) {
            return std::nullopt;
        }
        return SaveKey{.system = game.sourceId, .game = game.titleId};
    }
    return SaveKey{.system = game.sourceId, .game = game.file.stem().string()};
}

std::vector<SaveSlot> storedSlots(const SaveHome& home) {
    std::vector<SaveSlot> slots = home.layout == roms::SaveLayout::YuzuNand
                                      ? yuzuSlots(home.folder, home.config, home.titleId)
                                      : ryujinxSlots(home.folder, home.config, home.titleId);
    std::ranges::for_each(slots, measure);
    return slots;
}

bool copyable(const std::vector<EmulatorSaves>& saves) {
    return saves.size() >= 2 && std::ranges::any_of(saves, &EmulatorSaves::holdsFiles);
}

std::vector<EmulatorSaves> savesOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                   const fs::path& runnerSaves) {
    const std::optional<SaveKey> key = keyOf(game);
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
                                  .slots = runnerSlots(runnerSaves, game),
                                  .home = std::nullopt,
                                  .processHint = {}};
            std::ranges::for_each(saves.slots, measure);
            if (!saves.slots.empty()) {
                found.push_back(std::move(saves));
            }
            continue;
        }
        if (!entry->saves || key->system != switchSystem) {
            continue;
        }
        const bool flatpak = option.flatpak && !entry->saves->flatpakFolder.empty();
        saves.home =
            SaveHome{.layout = entry->saves->layout,
                     .folder = flatpak ? entry->saves->flatpakFolder : entry->saves->folder,
                     .config = flatpak ? entry->flatpakConfig : entry->config,
                     .titleId = key->game,
                     .saveData = game.switchSaveData};
        saves.processHint = option.processHint;
        saves.slots = storedSlots(*saves.home);
        found.push_back(std::move(saves));
    }
    return found;
}

} // namespace opensu::library::saves

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

std::vector<SaveSlot> storedSlots(const roms::SaveStore& store, const fs::path& config,
                                  const SaveKey& key) {
    std::vector<SaveSlot> slots;
    if (key.system == switchSystem) {
        slots = store.layout == roms::SaveLayout::YuzuNand
                    ? yuzuSlots(store.folder, config, key.game)
                    : ryujinxSlots(store.folder, config, key.game);
    }
    std::ranges::for_each(slots, measure);
    return slots;
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
        EmulatorSaves saves{.emulatorId = entry->id, .emulatorName = entry->name, .slots = {}};
        if (entry->builtIn()) {
            if (runnerListed) {
                continue;
            }
            runnerListed = true;
            saves = EmulatorSaves{.emulatorId = std::string{runnerSavesId},
                                  .emulatorName = std::string{runnerName},
                                  .slots = runnerSlots(runnerSaves, game)};
            std::ranges::for_each(saves.slots, measure);
        } else if (entry->saves) {
            saves.slots = storedSlots(*entry->saves, entry->config, *key);
        }
        if (!saves.slots.empty()) {
            found.push_back(std::move(saves));
        }
    }
    return found;
}

} // namespace opensu::library::saves

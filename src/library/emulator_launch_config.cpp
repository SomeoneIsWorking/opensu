#include "emulator_launch_config.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "azahar_config.hpp"
#include "cemu_pads.hpp"
#include "dolphin_config.hpp"
#include "duckstation_config.hpp"
#include "melonds_config.hpp"
#include "mgba_config.hpp"
#include "pcsx2_config.hpp"
#include "rpcs3_config.hpp"
#include "ryujinx_config.hpp"
#include "shadps4_config.hpp"
#include "xemu_config.hpp"
#include "yuzu_config.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

/// One emulator's rule: the files it may set, and what they say for a run.
struct LaunchRule {
    std::string_view id;
    std::vector<fs::path> (*files)(const LaunchSetup& setup);
    std::vector<RunFile> (*runFiles)(const LaunchSetup& setup);
};

constexpr std::array rules{
    LaunchRule{.id = cemuId, .files = cemuFiles, .runFiles = cemuRunFiles},
    LaunchRule{.id = dolphinId, .files = dolphinFiles, .runFiles = dolphinRunFiles},
    LaunchRule{.id = mgbaId, .files = mgbaFiles, .runFiles = mgbaRunFiles},
    LaunchRule{.id = yuzuLineIds[0], .files = yuzuFiles, .runFiles = yuzuRunFiles},
    LaunchRule{.id = yuzuLineIds[1], .files = yuzuFiles, .runFiles = yuzuRunFiles},
    LaunchRule{.id = yuzuLineIds[2], .files = yuzuFiles, .runFiles = yuzuRunFiles},
    LaunchRule{.id = ryujinxId, .files = ryujinxFiles, .runFiles = ryujinxRunFiles},
    LaunchRule{.id = azaharId, .files = azaharFiles, .runFiles = azaharRunFiles},
    LaunchRule{.id = pcsx2Id, .files = pcsx2Files, .runFiles = pcsx2RunFiles},
    LaunchRule{.id = melondsId, .files = melondsFiles, .runFiles = melondsRunFiles},
    LaunchRule{.id = rpcs3Id, .files = rpcs3Files, .runFiles = rpcs3RunFiles},
    LaunchRule{.id = duckstationId, .files = duckstationFiles, .runFiles = duckstationRunFiles},
    LaunchRule{.id = xemuId, .files = xemuFiles, .runFiles = xemuRunFiles},
    LaunchRule{.id = shadps4Id, .files = shadps4Files, .runFiles = shadps4RunFiles},
};

const LaunchRule* ruleOf(std::string_view id) {
    const auto rule = std::ranges::find(rules, id, &LaunchRule::id);
    return rule == rules.end() ? nullptr : &*rule;
}

} // namespace

std::optional<LaunchSetup> launchSetupOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                         const WiiControls& wii, std::string_view padGuid) {
    const auto option =
        std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    if (option == game.emulatorOptions.end() || ruleOf(option->id) == nullptr) {
        return std::nullopt;
    }
    const roms::EmulatorEntry* entry = catalog.find(option->id);
    if (entry == nullptr || entry->configOf(option->flatpak).empty()) {
        return std::nullopt;
    }
    return LaunchSetup{.emulatorId = option->id,
                       .config = entry->configOf(option->flatpak),
                       .data = entry->dataOf(option->flatpak),
                       .padGuid = std::string{padGuid},
                       .wiiRemote = game.sourceId == wiiSystem
                                        ? std::optional{wii.remoteFor(game.titleId)}
                                        : std::nullopt};
}

EmulatorLaunchConfig::EmulatorLaunchConfig(const LaunchSetup& setup) {
    const LaunchRule* rule = ruleOf(setup.emulatorId);
    if (rule == nullptr) {
        return;
    }
    LaunchFiles::recover(rule->files(setup));
    files_.emplace(rule->runFiles(setup));
}

} // namespace opensu::library

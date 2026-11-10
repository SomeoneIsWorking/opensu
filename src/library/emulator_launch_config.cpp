#include "emulator_launch_config.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "cemu_pads.hpp"
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
    LaunchRule{.id = yuzuLineIds[0], .files = yuzuFiles, .runFiles = yuzuRunFiles},
    LaunchRule{.id = yuzuLineIds[1], .files = yuzuFiles, .runFiles = yuzuRunFiles},
    LaunchRule{.id = yuzuLineIds[2], .files = yuzuFiles, .runFiles = yuzuRunFiles},
};

const LaunchRule* ruleOf(std::string_view id) {
    const auto* rule = std::ranges::find(rules, id, &LaunchRule::id);
    return rule == rules.end() ? nullptr : rule;
}

} // namespace

std::optional<LaunchSetup> launchSetupOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                         std::string_view padGuid) {
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
                       .padGuid = std::string{padGuid}};
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

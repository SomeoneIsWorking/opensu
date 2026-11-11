#include "emulator_launch_config.hpp"

#include <algorithm>
#include <array>
#include <system_error>
#include <utility>
#include <vector>

#include "azahar_config.hpp"
#include "cemu_pads.hpp"
#include "dolphin_config.hpp"
#include "duckstation_config.hpp"
#include "fileio/atomic_write.hpp"
#include "fileio/read_whole.hpp"
#include "lucent/log.h"
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

/// One emulator's rule: what it writes into the emulator's settings.
struct LaunchRule {
    std::string_view id;
    std::vector<SettingsWrite> (*writes)(const LaunchSetup& setup);
};

constexpr std::array rules{
    LaunchRule{.id = cemuId, .writes = cemuWrites},
    LaunchRule{.id = dolphinId, .writes = dolphinWrites},
    LaunchRule{.id = mgbaId, .writes = mgbaWrites},
    LaunchRule{.id = yuzuLineIds[0], .writes = yuzuWrites},
    LaunchRule{.id = yuzuLineIds[1], .writes = yuzuWrites},
    LaunchRule{.id = yuzuLineIds[2], .writes = yuzuWrites},
    LaunchRule{.id = ryujinxId, .writes = ryujinxWrites},
    LaunchRule{.id = azaharId, .writes = azaharWrites},
    LaunchRule{.id = pcsx2Id, .writes = pcsx2Writes},
    LaunchRule{.id = melondsId, .writes = melondsWrites},
    LaunchRule{.id = rpcs3Id, .writes = rpcs3Writes},
    LaunchRule{.id = duckstationId, .writes = duckstationWrites},
    LaunchRule{.id = xemuId, .writes = xemuWrites},
    LaunchRule{.id = shadps4Id, .writes = shadps4Writes},
};

const LaunchRule* ruleOf(std::string_view id) {
    const auto rule = std::ranges::find(rules, id, &LaunchRule::id);
    return rule == rules.end() ? nullptr : &*rule;
}

} // namespace

std::optional<LaunchSetup> launchSetupOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                         const WiiControls& wii, std::string_view padGuid,
                                         bool realWiiRemotes) {
    const auto option =
        std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    if (option == game.emulatorOptions.end() || ruleOf(option->id) == nullptr) {
        return std::nullopt;
    }
    const roms::EmulatorEntry* entry = catalog.find(option->id);
    if (entry == nullptr || entry->configOf(option->flatpak).empty()) {
        return std::nullopt;
    }
    const bool wiiGame = game.sourceId == wiiSystem;
    return LaunchSetup{.emulatorId = option->id,
                       .config = entry->configOf(option->flatpak),
                       .data = entry->dataOf(option->flatpak),
                       .padGuid = std::string{padGuid},
                       .wiiRemote =
                           wiiGame ? std::optional{wii.remoteFor(game.titleId)} : std::nullopt,
                       .realWiiRemotes = wiiGame && realWiiRemotes};
}

std::optional<SettingsWrite>
settingsWriteOf(const fs::path& file,
                const std::function<std::string(std::string_view)>& settings) {
    std::string now;
    std::error_code missing;
    if (fs::exists(file, missing)) {
        std::string error;
        std::optional<std::string> read = fileio::readWhole(file, error);
        if (!read) {
            lucent::warn("emulator", "{} is left as it is: {}", file.string(), error);
            return std::nullopt;
        }
        now = std::move(*read);
    }
    std::string bytes = settings(now);
    if (bytes == now) {
        return std::nullopt;
    }
    return SettingsWrite{.path = file, .bytes = std::move(bytes)};
}

void writeLaunchSettings(const LaunchSetup& setup) {
    const LaunchRule* rule = ruleOf(setup.emulatorId);
    if (rule == nullptr) {
        return;
    }
    for (const SettingsWrite& write : rule->writes(setup)) {
        std::string error;
        if (!fileio::writeWhole(write.path, write.bytes, error)) {
            lucent::warn("emulator", "{} is left as it was: {}", write.path.string(), error);
            continue;
        }
        lucent::info("emulator", "{} written for the launch", write.path.string());
    }
}

} // namespace opensu::library

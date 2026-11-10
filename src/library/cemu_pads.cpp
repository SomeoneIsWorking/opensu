#include "cemu_pads.hpp"

#include <algorithm>
#include <system_error>

#include "fileio/atomic_write.hpp"
#include "fileio/read_whole.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view blockOpen{"<controller>"};
constexpr std::string_view blockClose{"</controller>"};
constexpr std::string_view sdlApi{"<api>SDLController</api>"};
constexpr std::string_view uuidOpen{"<uuid>"};
constexpr std::string_view uuidClose{"</uuid>"};

fs::path profileOf(const fs::path& profiles, int controller) {
    return profiles / ("controller" + std::to_string(controller) + ".xml");
}

fs::path keptOf(const fs::path& profile) {
    fs::path kept = profile;
    kept += CemuPads::keptSuffix;
    return kept;
}

/// Puts the kept player's `profile` back over the bound one.
void putBack(const fs::path& profile) {
    std::error_code failed;
    fs::rename(keptOf(profile), profile, failed);
    if (failed) {
        lucent::error("cemu", "cannot put the player's {} back: {}", profile.string(),
                      failed.message());
    }
}

} // namespace

std::string reboundCemuProfile(std::string_view profile, std::string_view uuid) {
    std::string out;
    out.reserve(profile.size());
    std::size_t at = 0;
    while (true) {
        const std::size_t start = profile.find(blockOpen, at);
        const std::size_t end =
            start == std::string_view::npos ? start : profile.find(blockClose, start);
        if (end == std::string_view::npos) {
            break;
        }
        const std::string_view block = profile.substr(start, end - start);
        const std::size_t value = block.find(uuidOpen);
        const std::size_t valueEnd =
            value == std::string_view::npos ? value : block.find(uuidClose, value);
        if (block.find(sdlApi) == std::string_view::npos || valueEnd == std::string_view::npos) {
            out.append(profile.substr(at, end - at));
        } else {
            const std::size_t from = start + value + uuidOpen.size();
            out.append(profile.substr(at, from - at));
            out.append(uuid);
            out.append(profile.substr(start + valueEnd, end - start - valueEnd));
        }
        at = end;
    }
    out.append(profile.substr(at));
    return out;
}

std::optional<CemuBinding> cemuBindingOf(const Game& game, const roms::EmulatorCatalog& catalog,
                                         std::string_view padGuid) {
    if (padGuid.empty()) {
        return std::nullopt;
    }
    const auto option =
        std::ranges::find(game.emulatorOptions, game.emulator, &EmulatorOption::name);
    if (option == game.emulatorOptions.end() || option->id != cemuId) {
        return std::nullopt;
    }
    const roms::EmulatorEntry* entry = catalog.find(option->id);
    if (entry == nullptr || entry->configOf(option->flatpak).empty()) {
        return std::nullopt;
    }
    return CemuBinding{.config = entry->configOf(option->flatpak), .padGuid = std::string{padGuid}};
}

CemuPads::CemuPads(const CemuBinding& binding) : profiles_{binding.config / "controllerProfiles"} {
    restore(profiles_);
    for (int controller = 0; controller < controllers; ++controller) {
        const fs::path profile = profileOf(profiles_, controller);
        std::error_code missing;
        if (!fs::exists(profile, missing)) {
            continue;
        }
        std::string error;
        const std::optional<std::string> player = fileio::readWhole(profile, error);
        if (!player) {
            lucent::warn("cemu", "{} stays the player's: {}", profile.string(), error);
            continue;
        }
        const std::string uuid = std::to_string(controller) + "_" + binding.padGuid;
        const std::string bound = reboundCemuProfile(*player, uuid);
        if (bound == *player) {
            continue;
        }
        std::error_code failed;
        fs::rename(profile, keptOf(profile), failed);
        if (failed) {
            lucent::warn("cemu", "{} stays the player's: {}", profile.string(), failed.message());
            continue;
        }
        if (!fileio::writeWhole(profile, bound, error)) {
            lucent::warn("cemu", "{} stays the player's: {}", profile.string(), error);
            putBack(profile);
            continue;
        }
        bound_.push_back(profile);
        lucent::info("cemu", "controller {} bound to virtual pad {}", controller, uuid);
    }
}

CemuPads::~CemuPads() {
    for (const fs::path& profile : bound_) {
        putBack(profile);
    }
    if (!bound_.empty()) {
        lucent::info("cemu", "the player's {} controller profiles put back", bound_.size());
    }
}

void CemuPads::restore(const fs::path& profiles) {
    for (int controller = 0; controller < controllers; ++controller) {
        const fs::path profile = profileOf(profiles, controller);
        std::error_code missing;
        if (!fs::exists(keptOf(profile), missing)) {
            continue;
        }
        lucent::info("cemu", "putting back the player's {} a crashed run left kept",
                     profile.string());
        putBack(profile);
    }
}

} // namespace opensu::library

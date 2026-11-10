#include "cemu_pads.hpp"

#include <optional>

#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view blockOpen{"<controller>"};
constexpr std::string_view blockClose{"</controller>"};
constexpr std::string_view sdlApi{"<api>SDLController</api>"};
constexpr std::string_view uuidOpen{"<uuid>"};
constexpr std::string_view uuidClose{"</uuid>"};

fs::path profileOf(const fs::path& config, int controller) {
    return config / "controllerProfiles" / ("controller" + std::to_string(controller) + ".xml");
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

std::vector<fs::path> cemuFiles(const LaunchSetup& setup) {
    const fs::path& config = setup.config;
    std::vector<fs::path> files;
    files.reserve(cemuControllers);
    for (int controller = 0; controller < cemuControllers; ++controller) {
        files.push_back(profileOf(config, controller));
    }
    return files;
}

std::vector<RunFile> cemuRunFiles(const LaunchSetup& setup) {
    const fs::path& config = setup.config;
    const std::string_view padGuid = setup.padGuid;
    std::vector<RunFile> files;
    if (padGuid.empty()) {
        return files;
    }
    for (int controller = 0; controller < cemuControllers; ++controller) {
        const std::string uuid = std::to_string(controller) + "_" + std::string{padGuid};
        std::optional<RunFile> run =
            runFileOf(profileOf(config, controller), [&](std::string_view player) {
                return reboundCemuProfile(player, uuid);
            });
        if (!run) {
            continue;
        }
        lucent::info("cemu", "controller {} bound to virtual pad {}", controller, uuid);
        files.push_back(std::move(*run));
    }
    return files;
}

} // namespace opensu::library

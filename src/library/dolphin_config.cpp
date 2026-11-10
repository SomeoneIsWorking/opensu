#include "dolphin_config.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <system_error>

#include "fileio/read_whole.hpp"
#include "ini_file.hpp"
#include "lucent/log.h"
#include "lucent/text.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view deviceKey{"Device"};
constexpr std::string_view sdlSource{"SDL/"};
constexpr std::string_view profileSection{"Profile"};
constexpr std::string_view iniSuffix{".ini"};

/// One of Dolphin's settings files for an emulated controller, and its per-game profiles.
struct ControllerFiles {
    DolphinController controller;
    /// Its settings file in the settings folder, sections `<section><N>`.
    std::string_view file;
    std::string_view section;
    /// `GameSettings` `[Controls]` `<key><N>` names a profile in `Profiles/<folder>`.
    std::string_view profileKey;
    std::string_view folder;
};

constexpr std::array controllers{
    ControllerFiles{.controller = DolphinController::GameCube,
                    .file = "GCPadNew.ini",
                    .section = "GCPad",
                    .profileKey = "PadProfile",
                    .folder = "GCPad"},
    ControllerFiles{.controller = DolphinController::WiiRemote,
                    .file = "WiimoteNew.ini",
                    .section = "Wiimote",
                    .profileKey = "WiimoteProfile",
                    .folder = "Wiimote"},
};

constexpr std::string_view mainFile{"Dolphin.ini"};
constexpr IniKey analyticsAsked{.section = "Analytics", .key = "PermissionAsked"};

struct Mapping {
    std::string_view key;
    std::string_view value;
};

// A GameCube pad laid out as it sits: A south, B west, X east, Y north, Z on the right shoulder.
// Input names are Dolphin's for an SDL game controller; vertical axes read up as `+`.
constexpr std::array gameCubePad{
    Mapping{"Buttons/A", "`Button S`"},
    Mapping{"Buttons/B", "`Button W`"},
    Mapping{"Buttons/X", "`Button E`"},
    Mapping{"Buttons/Y", "`Button N`"},
    Mapping{"Buttons/Z", "`Shoulder R`"},
    Mapping{"Buttons/Start", "`Start`"},
    Mapping{"Main Stick/Up", "`Left Y+`"},
    Mapping{"Main Stick/Down", "`Left Y-`"},
    Mapping{"Main Stick/Left", "`Left X-`"},
    Mapping{"Main Stick/Right", "`Left X+`"},
    Mapping{"C-Stick/Up", "`Right Y+`"},
    Mapping{"C-Stick/Down", "`Right Y-`"},
    Mapping{"C-Stick/Left", "`Right X-`"},
    Mapping{"C-Stick/Right", "`Right X+`"},
    Mapping{"Triggers/L", "`Trigger L`"},
    Mapping{"Triggers/R", "`Trigger R`"},
    Mapping{"Triggers/L-Analog", "`Trigger L`"},
    Mapping{"Triggers/R-Analog", "`Trigger R`"},
    Mapping{"D-Pad/Up", "`Pad N`"},
    Mapping{"D-Pad/Down", "`Pad S`"},
    Mapping{"D-Pad/Left", "`Pad W`"},
    Mapping{"D-Pad/Right", "`Pad E`"},
};

std::string virtualPad(int player) {
    std::string device{sdlSource};
    device += std::to_string(player - 1);
    device += '/';
    device += virtualPadSdlName;
    return device;
}

std::string slotSection(const ControllerFiles& files, int player) {
    std::string section{files.section};
    section += std::to_string(player);
    return section;
}

/// A per-game profile some game's settings name, for the player whose slot names it first.
struct NamedProfile {
    fs::path path;
    DolphinController controller;
    int player;
};

std::vector<fs::path> sortedInis(const fs::path& folder, bool recursive) {
    std::vector<fs::path> found;
    std::error_code failed;
    const auto add = [&found](const fs::directory_entry& entry) {
        std::error_code unread;
        if (entry.path().extension() == iniSuffix && entry.is_regular_file(unread)) {
            found.push_back(entry.path());
        }
    };
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator{folder, failed}) {
            add(entry);
        }
    } else {
        for (const auto& entry : fs::directory_iterator{folder, failed}) {
            add(entry);
        }
    }
    std::ranges::sort(found);
    return found;
}

/// The profiles a `[Controls]` setting names, as Dolphin's `GetProfilesFromSetting` reads it:
/// comma-separated names under `root`, each a folder of profiles or a profile without `.ini`.
std::vector<fs::path> profilesOf(std::string_view setting, const fs::path& root) {
    std::vector<fs::path> found;
    std::size_t at = 0;
    while (at <= setting.size()) {
        const std::size_t comma = std::min(setting.find(',', at), setting.size());
        const std::string_view choice = lucent::text::trim(setting.substr(at, comma - at));
        at = comma + 1;
        if (choice.empty()) {
            continue;
        }
        const fs::path path = root / std::string{choice};
        std::error_code missing;
        if (fs::is_directory(path, missing)) {
            const std::vector<fs::path> inside = sortedInis(path, true);
            found.insert(found.end(), inside.begin(), inside.end());
            continue;
        }
        fs::path file = path;
        file += iniSuffix;
        if (fs::is_regular_file(file, missing)) {
            found.push_back(std::move(file));
        }
    }
    return found;
}

std::vector<NamedProfile> namedProfiles(const LaunchSetup& setup) {
    std::vector<NamedProfile> named;
    if (setup.data.empty()) {
        return named;
    }
    for (const fs::path& game : sortedInis(setup.data / "GameSettings", false)) {
        std::string error;
        const std::optional<std::string> text = fileio::readWhole(game, error);
        if (!text) {
            lucent::warn("dolphin", "{} is not read: {}", game.string(), error);
            continue;
        }
        for (const ControllerFiles& files : controllers) {
            const fs::path root = setup.config / "Profiles" / std::string{files.folder};
            for (int player = 1; player <= dolphinSlots; ++player) {
                std::string key{files.profileKey};
                key += std::to_string(player);
                const std::optional<std::string> setting =
                    iniValueIn(*text, IniKey{.section = "Controls", .key = key});
                if (!setting) {
                    continue;
                }
                for (fs::path& path : profilesOf(*setting, root)) {
                    if (std::ranges::find(named, path, &NamedProfile::path) == named.end()) {
                        named.push_back(NamedProfile{.path = std::move(path),
                                                     .controller = files.controller,
                                                     .player = player});
                    }
                }
            }
        }
    }
    return named;
}

void addRun(std::vector<RunFile>& files, const fs::path& file,
            const std::function<std::string(std::string_view)>& run) {
    if (std::optional<RunFile> set = runFileOf(file, run)) {
        files.push_back(std::move(*set));
    }
}

} // namespace

std::string dolphinSlotSettings(std::string_view ini, std::string_view section,
                                DolphinController controller, int player) {
    const IniKey device{.section = section, .key = deviceKey};
    if (iniValueIn(ini, device).value_or("").starts_with(sdlSource)) {
        return withIniValue(ini, device, virtualPad(player));
    }
    if (controller == DolphinController::WiiRemote) {
        return std::string{ini};
    }
    std::string out = withIniValue(ini, device, virtualPad(player));
    for (const Mapping& mapping : gameCubePad) {
        out = withIniValue(out, IniKey{.section = section, .key = mapping.key}, mapping.value);
    }
    return out;
}

std::vector<fs::path> dolphinFiles(const LaunchSetup& setup) {
    std::vector<fs::path> files{setup.config / mainFile};
    for (const ControllerFiles& controller : controllers) {
        files.push_back(setup.config / controller.file);
    }
    for (NamedProfile& profile : namedProfiles(setup)) {
        files.push_back(std::move(profile.path));
    }
    return files;
}

std::vector<RunFile> dolphinRunFiles(const LaunchSetup& setup) {
    std::vector<RunFile> files;
    addRun(files, setup.config / mainFile, [](std::string_view settings) {
        if (iniValueIn(settings, analyticsAsked) == "True") {
            return std::string{settings};
        }
        return withIniValue(settings, analyticsAsked, "True");
    });
    if (setup.padGuid.empty()) {
        return files;
    }
    for (const ControllerFiles& controller : controllers) {
        addRun(files, setup.config / controller.file, [&controller](std::string_view settings) {
            std::string run{settings};
            for (int slot = 1; slot <= dolphinSlots; ++slot) {
                run = dolphinSlotSettings(run, slotSection(controller, slot), controller.controller,
                                          slot);
            }
            return run;
        });
    }
    for (const NamedProfile& profile : namedProfiles(setup)) {
        addRun(files, profile.path, [&profile](std::string_view settings) {
            return dolphinSlotSettings(settings, profileSection, profile.controller,
                                       profile.player);
        });
    }
    return files;
}

} // namespace opensu::library

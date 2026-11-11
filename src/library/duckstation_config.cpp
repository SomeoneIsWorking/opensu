#include "duckstation_config.hpp"

#include <array>
#include <optional>
#include <system_error>

#include "ini_file.hpp"
#include "sdl_pad_ini.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view analogController{"AnalogController"};
constexpr std::array standardTypes{std::string_view{"DigitalController"}, analogController};

/// DuckStation's own mapping (`SDLInputSource::GetGenericBindingMapping`), without Analog, which
/// is the Guide button.
constexpr std::array defaults{
    SdlPadDefault{"Up", "DPadUp"},
    SdlPadDefault{"Right", "DPadRight"},
    SdlPadDefault{"Down", "DPadDown"},
    SdlPadDefault{"Left", "DPadLeft"},
    SdlPadDefault{"Triangle", "Y"},
    SdlPadDefault{"Circle", "B"},
    SdlPadDefault{"Cross", "A"},
    SdlPadDefault{"Square", "X"},
    SdlPadDefault{"Select", "Back"},
    SdlPadDefault{"Start", "Start"},
    SdlPadDefault{"L1", "LeftShoulder"},
    SdlPadDefault{"L2", "+LeftTrigger"},
    SdlPadDefault{"R1", "RightShoulder"},
    SdlPadDefault{"R2", "+RightTrigger"},
    SdlPadDefault{"L3", "LeftStick"},
    SdlPadDefault{"R3", "RightStick"},
    SdlPadDefault{"LUp", "-LeftY"},
    SdlPadDefault{"LRight", "+LeftX"},
    SdlPadDefault{"LDown", "+LeftY"},
    SdlPadDefault{"LLeft", "-LeftX"},
    SdlPadDefault{"RUp", "-RightY"},
    SdlPadDefault{"RRight", "+RightX"},
    SdlPadDefault{"RDown", "+RightY"},
    SdlPadDefault{"RLeft", "-RightX"},
    SdlPadDefault{"LargeMotor", "LargeMotor"},
    SdlPadDefault{"SmallMotor", "SmallMotor"},
};

// Pad 1 without a type is DuckStation's default DualShock; the others default to none.
constexpr SdlPadLayout layout{.emulator = "duckstation",
                              .pads = duckstationPads,
                              .standardTypes = standardTypes,
                              .firstPadType = analogController,
                              .defaults = defaults};

} // namespace

std::string duckstationPadSettings(std::string_view ini) {
    return sdlPadSettings(ini, layout);
}

std::string duckstationLaunchSettings(std::string_view ini, bool virtualPads) {
    std::string out = withIniValue(ini, {"AutoUpdater", "CheckAtStartup"}, "false");
    out = withIniValue(out, {"Main", "SetupWizardIncomplete"}, "false");
    // Its launcher-shortcut question opens before the boot from an AppImage without a desktop file.
    out = withIniValue(out, {"Main", "NoDesktopFile"}, "true");
    return virtualPads ? duckstationPadSettings(out) : out;
}

std::vector<fs::path> duckstationFiles(const LaunchSetup& setup) {
    std::vector<fs::path> files{setup.config / duckstationSettingsFile};
    std::error_code error;
    fs::directory_iterator entry(setup.config / duckstationProfilesFolder, error);
    for (; !error && entry != fs::directory_iterator{}; entry.increment(error)) {
        if (entry->path().extension() == ".ini") {
            files.push_back(entry->path());
        }
    }
    return files;
}

std::vector<RunFile> duckstationRunFiles(const LaunchSetup& setup) {
    const bool virtualPads = !setup.padGuid.empty();
    std::vector<RunFile> files;
    for (const fs::path& file : duckstationFiles(setup)) {
        std::optional<RunFile> run =
            file == setup.config / duckstationSettingsFile
                ? runFileOf(file,
                            [&](std::string_view ini) {
                                return duckstationLaunchSettings(ini, virtualPads);
                            })
                : runFileOf(file, [&](std::string_view ini) {
                      return virtualPads ? duckstationPadSettings(ini) : std::string{ini};
                  });
        if (run) {
            files.push_back(std::move(*run));
        }
    }
    return files;
}

} // namespace opensu::library

#include "pcsx2_config.hpp"

#include <array>
#include <optional>
#include <system_error>

#include "ini_file.hpp"
#include "sdl_pad_ini.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view dualShock2{"DualShock2"};
constexpr std::array standardTypes{dualShock2};

/// PCSX2's own mapping (`SDLInputSource::GetGenericBindingMapping`), without Analog, which is the
/// Guide button.
constexpr std::array defaults{
    SdlPadDefault{"Up", "DPadUp"},
    SdlPadDefault{"Right", "DPadRight"},
    SdlPadDefault{"Down", "DPadDown"},
    SdlPadDefault{"Left", "DPadLeft"},
    SdlPadDefault{"Triangle", "FaceNorth"},
    SdlPadDefault{"Circle", "FaceEast"},
    SdlPadDefault{"Cross", "FaceSouth"},
    SdlPadDefault{"Square", "FaceWest"},
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

constexpr SdlPadLayout layout{.emulator = "pcsx2",
                              .pads = pcsx2Pads,
                              .standardTypes = standardTypes,
                              .firstPadType = dualShock2,
                              .defaults = defaults};

/// The settings files the rule reads under `setup.config`.
std::vector<fs::path> settingsFiles(const LaunchSetup& setup) {
    std::vector<fs::path> files{setup.config / pcsx2SettingsFile};
    std::error_code error;
    fs::directory_iterator entry(setup.config / pcsx2ProfilesFolder, error);
    for (; !error && entry != fs::directory_iterator{}; entry.increment(error)) {
        if (entry->path().extension() == ".ini") {
            files.push_back(entry->path());
        }
    }
    return files;
}

} // namespace

std::string pcsx2PadSettings(std::string_view ini) {
    return sdlPadSettings(ini, layout);
}

std::string pcsx2LaunchSettings(std::string_view ini, bool virtualPads) {
    std::string out{ini};
    // A file without it is reset to PCSX2's defaults and the wizard opens on it.
    if (!iniValueIn(out, {"UI", "SettingsVersion"})) {
        out = withIniValue(out, {"UI", "SettingsVersion"}, "1");
    }
    out = withIniValue(out, {"UI", "SetupWizardIncomplete"}, "false");
    return virtualPads ? pcsx2PadSettings(out) : out;
}

std::vector<SettingsWrite> pcsx2Writes(const LaunchSetup& setup) {
    const bool virtualPads = !setup.padGuid.empty();
    std::vector<SettingsWrite> files;
    for (const fs::path& file : settingsFiles(setup)) {
        std::optional<SettingsWrite> run =
            file == setup.config / pcsx2SettingsFile
                ? settingsWriteOf(file,
                                  [&](std::string_view ini) {
                                      return pcsx2LaunchSettings(ini, virtualPads);
                                  })
                : settingsWriteOf(file, [&](std::string_view ini) {
                      return virtualPads ? pcsx2PadSettings(ini) : std::string{ini};
                  });
        if (run) {
            files.push_back(std::move(*run));
        }
    }
    return files;
}

} // namespace opensu::library

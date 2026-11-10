#include "pcsx2_config.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <system_error>

#include "ini_file.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view sdlDevice{"SDL-"};
constexpr std::string_view dualShock2{"DualShock2"};

/// A DualShock 2 input and the SDL gamepad input PCSX2's own mapping gives it
/// (`SDLInputSource::GetGenericBindingMapping`), without Analog, which is the Guide button.
struct DefaultBinding {
    std::string_view key;
    std::string_view input;
};

constexpr std::array defaults{
    DefaultBinding{"Up", "DPadUp"},
    DefaultBinding{"Right", "DPadRight"},
    DefaultBinding{"Down", "DPadDown"},
    DefaultBinding{"Left", "DPadLeft"},
    DefaultBinding{"Triangle", "FaceNorth"},
    DefaultBinding{"Circle", "FaceEast"},
    DefaultBinding{"Cross", "FaceSouth"},
    DefaultBinding{"Square", "FaceWest"},
    DefaultBinding{"Select", "Back"},
    DefaultBinding{"Start", "Start"},
    DefaultBinding{"L1", "LeftShoulder"},
    DefaultBinding{"L2", "+LeftTrigger"},
    DefaultBinding{"R1", "RightShoulder"},
    DefaultBinding{"R2", "+RightTrigger"},
    DefaultBinding{"L3", "LeftStick"},
    DefaultBinding{"R3", "RightStick"},
    DefaultBinding{"LUp", "-LeftY"},
    DefaultBinding{"LRight", "+LeftX"},
    DefaultBinding{"LDown", "+LeftY"},
    DefaultBinding{"LLeft", "-LeftX"},
    DefaultBinding{"RUp", "-RightY"},
    DefaultBinding{"RRight", "+RightX"},
    DefaultBinding{"RDown", "+RightY"},
    DefaultBinding{"RLeft", "-RightX"},
    DefaultBinding{"LargeMotor", "LargeMotor"},
    DefaultBinding{"SmallMotor", "SmallMotor"},
};

/// `binding` with each `SDL-<n>/` made `device`.
std::string retargeted(std::string_view binding, std::string_view device) {
    std::string out;
    std::size_t at = 0;
    while (true) {
        const std::size_t found = binding.find(sdlDevice, at);
        if (found == std::string_view::npos) {
            out.append(binding.substr(at));
            return out;
        }
        std::size_t digits = found + sdlDevice.size();
        while (digits < binding.size() &&
               std::isdigit(static_cast<unsigned char>(binding[digits])) != 0) {
            ++digits;
        }
        out.append(binding.substr(at, found - at));
        if (digits == found + sdlDevice.size() || digits == binding.size() ||
            binding[digits] != '/') {
            out.append(binding.substr(found, digits - found));
        } else {
            out.append(device);
        }
        at = digits;
    }
}

std::string withPad(std::string ini, int pad) {
    const std::string section = "Pad" + std::to_string(pad + 1);
    const std::string device = std::string{sdlDevice} + std::to_string(pad);
    const std::vector<std::string> keys = iniKeysIn(ini, section);
    const auto valueOf = [&](const std::string& key) {
        return iniValueIn(ini, {section, key}).value_or("");
    };
    const bool hasSdl = std::ranges::any_of(keys, [&](const std::string& key) {
        return key != "Type" && valueOf(key).find(sdlDevice) != std::string::npos;
    });
    if (hasSdl) {
        for (const std::string& key : keys) {
            const std::string value = valueOf(key);
            if (key != "Type" && value.find(sdlDevice) != std::string::npos) {
                ini = withIniValue(ini, {section, key}, retargeted(value, device));
            }
        }
        lucent::info("pcsx2", "pad {} bound to {}", pad + 1, device);
        return ini;
    }
    const std::optional<std::string> type = iniValueIn(ini, {section, "Type"});
    if (type.value_or(pad == 0 ? std::string{dualShock2} : "") != dualShock2) {
        return ini;
    }
    ini = withIniValue(ini, {section, "Type"}, dualShock2);
    for (const DefaultBinding& binding : defaults) {
        std::string value = device;
        value += '/';
        value += binding.input;
        ini = withIniValue(ini, {section, binding.key}, value);
    }
    lucent::info("pcsx2", "pad {} given the standard layout on {}", pad + 1, device);
    return ini;
}

} // namespace

std::string pcsx2PadSettings(std::string_view ini) {
    std::string out{ini};
    for (int pad = 0; pad < pcsx2Pads; ++pad) {
        out = withPad(std::move(out), pad);
    }
    return out;
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

std::vector<fs::path> pcsx2Files(const LaunchSetup& setup) {
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

std::vector<RunFile> pcsx2RunFiles(const LaunchSetup& setup) {
    const bool virtualPads = !setup.padGuid.empty();
    std::vector<RunFile> files;
    for (const fs::path& file : pcsx2Files(setup)) {
        std::optional<RunFile> run =
            file == setup.config / pcsx2SettingsFile
                ? runFileOf(file,
                            [&](std::string_view ini) {
                                return pcsx2LaunchSettings(ini, virtualPads);
                            })
                : runFileOf(file, [&](std::string_view ini) {
                      return virtualPads ? pcsx2PadSettings(ini) : std::string{ini};
                  });
        if (run) {
            files.push_back(std::move(*run));
        }
    }
    return files;
}

} // namespace opensu::library

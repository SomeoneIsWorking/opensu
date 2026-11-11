#include "mgba_config.hpp"

#include <array>
#include <optional>

#include "ini_file.hpp"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

struct Binding {
    std::string_view key;
    std::string_view value;
};

// mGBA 0.10's own profile for xpad (`InputProfile::s_defaultMaps`), in SDL joystick numbers: GBA A
// on xpad B (button 1), B on A, Select on Back, Start on Start, the shoulders on the bumpers; the
// d-pad on hat 0 and the left stick (axis 0 and 1), as `mSDLInitBindingsGBA` binds them.
constexpr std::array profile{
    Binding{"keyA", "1"},           Binding{"keyB", "0"},
    Binding{"keySelect", "6"},      Binding{"keyStart", "7"},
    Binding{"keyL", "4"},           Binding{"keyR", "5"},
    Binding{"hat0Up", "6"},         Binding{"hat0Right", "4"},
    Binding{"hat0Down", "7"},       Binding{"hat0Left", "5"},
    Binding{"axisLeftAxis", "-0"},  Binding{"axisLeftValue", "-16384"},
    Binding{"axisRightAxis", "+0"}, Binding{"axisRightValue", "16384"},
    Binding{"axisUpAxis", "-1"},    Binding{"axisUpValue", "-16384"},
    Binding{"axisDownAxis", "+1"},  Binding{"axisDownValue", "16384"},
};

} // namespace

std::string mgbaLaunchSettings(std::string_view ini) {
    std::string section{"gba.input-profile."};
    section += virtualPadSdlName;
    std::string out{ini};
    for (const Binding& binding : profile) {
        const IniKey key{.section = section, .key = binding.key};
        if (!iniValueIn(out, key)) {
            out = withIniValue(out, key, binding.value);
        }
    }
    return out;
}

std::vector<SettingsWrite> mgbaWrites(const LaunchSetup& setup) {
    if (setup.padGuid.empty()) {
        return {};
    }
    std::optional<SettingsWrite> run =
        settingsWriteOf(setup.config / mgbaSettingsFile, mgbaLaunchSettings);
    if (!run) {
        return {};
    }
    return {std::move(*run)};
}

} // namespace opensu::library

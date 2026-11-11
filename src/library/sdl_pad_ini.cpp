#include "sdl_pad_ini.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

#include "ini_file.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

constexpr std::string_view sdlDevice{"SDL-"};

/// Whether `c` can be part of an SDL device: a player index or a persistent identifier
/// (DuckStation's `<GUID>-<serial>` or `SB-<GUID>-<hex>`).
bool deviceCharacter(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-';
}

std::string withPad(std::string ini, int pad, const SdlPadLayout& layout) {
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
                ini = withIniValue(ini, {section, key}, sdlBindingOn(value, device));
            }
        }
        lucent::info(layout.emulator, "pad {} bound to {}", pad + 1, device);
        return ini;
    }
    const std::string type = iniValueIn(ini, {section, "Type"})
                                 .value_or(pad == 0 ? std::string{layout.firstPadType} : "");
    if (std::ranges::find(layout.standardTypes, type) == layout.standardTypes.end()) {
        return ini;
    }
    ini = withIniValue(ini, {section, "Type"}, type);
    for (const SdlPadDefault& binding : layout.defaults) {
        std::string value = device;
        value += '/';
        value += binding.input;
        ini = withIniValue(ini, {section, binding.key}, value);
    }
    lucent::info(layout.emulator, "pad {} given the standard layout on {}", pad + 1, device);
    return ini;
}

} // namespace

std::string sdlBindingOn(std::string_view binding, std::string_view device) {
    std::string out;
    std::size_t at = 0;
    while (true) {
        const std::size_t found = binding.find(sdlDevice, at);
        if (found == std::string_view::npos) {
            out.append(binding.substr(at));
            return out;
        }
        std::size_t end = found + sdlDevice.size();
        while (end < binding.size() && deviceCharacter(binding[end])) {
            ++end;
        }
        out.append(binding.substr(at, found - at));
        if (end == found + sdlDevice.size() || end == binding.size() || binding[end] != '/') {
            out.append(binding.substr(found, end - found));
        } else {
            out.append(device);
        }
        at = end;
    }
}

std::string sdlPadSettings(std::string_view ini, const SdlPadLayout& layout) {
    std::string out{ini};
    for (int pad = 0; pad < layout.pads; ++pad) {
        out = withPad(std::move(out), pad, layout);
    }
    return out;
}

} // namespace opensu::library

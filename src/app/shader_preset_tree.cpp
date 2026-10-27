#include "shader_preset_tree.hpp"

#include <algorithm>

namespace opensu::app {
namespace {

constexpr std::string_view crumb = " › ";

/// Whether `path` lies below `folder` ("" the top), with `rest` its path from there.
bool below(std::string_view path, std::string_view folder, std::string_view& rest) {
    if (folder.empty()) {
        rest = path;
        return true;
    }
    if (path.size() <= folder.size() || !path.starts_with(folder) || path[folder.size()] != '/') {
        return false;
    }
    rest = path.substr(folder.size() + 1);
    return true;
}

} // namespace

std::string presetCrumbs(std::string_view folder) {
    std::string out;
    for (const char c : folder) {
        if (c == '/') {
            out += crumb;
        } else {
            out += c;
        }
    }
    return out;
}

bool presetBelow(std::string_view path, std::string_view folder) {
    std::string_view rest;
    return below(path, folder, rest);
}

PresetFolder presetFolder(const std::vector<library::roms::ShaderPreset>& presets,
                          std::string_view folder) {
    PresetFolder shown;
    for (const library::roms::ShaderPreset& preset : presets) {
        std::string_view rest;
        if (preset.folder.empty() || preset.folder == folder ||
            !below(preset.folder, folder, rest)) {
            continue;
        }
        const std::string child{rest.substr(0, rest.find('/'))};
        const auto at = std::ranges::find(shown.folders, child, &PresetFolder::Child::name);
        if (at == shown.folders.end()) {
            shown.folders.push_back(PresetFolder::Child{
                child, folder.empty() ? child : std::string{folder} + "/" + child, 1});
        } else {
            ++at->presets;
        }
    }
    for (const library::roms::ShaderPreset& preset : presets) {
        if (preset.folder == folder) {
            shown.presets.push_back(preset);
        }
    }
    return shown;
}

std::string presetLabel(std::string_view value) {
    if (value.empty()) {
        return "None";
    }
    const std::size_t slash = value.rfind('/');
    return (slash == std::string_view::npos
                ? std::string{}
                : presetCrumbs(value.substr(0, slash)) + std::string{crumb}) +
           library::roms::presetName(value);
}

} // namespace opensu::app

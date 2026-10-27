// shader_preset_tree — the downloaded shader pack as a folder tree (crt, handheld, bezel ›
// Mega_Bezel › ...): what one folder holds (its subfolders, each with how many presets lie below
// it, then its own presets), and a preset value as a row reads it. The layered editor's shader
// picker lists it.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "library/shader_pack.hpp"

namespace opensu::app {

/// One folder of the pack.
struct PresetFolder {
    struct Child {
        std::string name;
        /// Its path in the pack.
        std::string path;
        /// The presets anywhere below it.
        std::size_t presets{0};
    };
    std::vector<Child> folders;
    std::vector<library::roms::ShaderPreset> presets;
};

/// What `folder` ("" the top) holds among `presets`, in pack order.
[[nodiscard]] PresetFolder presetFolder(const std::vector<library::roms::ShaderPreset>& presets,
                                        std::string_view folder);

/// Whether preset `path` lies below `folder`.
[[nodiscard]] bool presetBelow(std::string_view path, std::string_view folder);

/// `folder` as a breadcrumb: "bezel › Mega_Bezel".
[[nodiscard]] std::string presetCrumbs(std::string_view folder);

/// A preset value as a row reads it: "None" for "", else its folders and name ("crt ›
/// crt-royale").
[[nodiscard]] std::string presetLabel(std::string_view value);

} // namespace opensu::app

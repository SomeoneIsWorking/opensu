#include "shader_preset_picker.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view prefix = "shaders.";
constexpr std::string_view clearId = "shaders.clear";
constexpr std::string_view noneId = "shaders.none";
constexpr std::string_view folderId = "shaders.folder.";
constexpr std::string_view presetId = "shaders.preset.";
constexpr std::string_view crumb = " › ";

/// `folder` as a breadcrumb: "bezel › Mega_Bezel".
std::string crumbs(std::string_view folder) {
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

std::string presetsCount(std::size_t count) {
    return std::to_string(count) + (count == 1 ? " preset" : " presets");
}

} // namespace

std::string presetLabel(std::string_view value) {
    if (value.empty()) {
        return "None";
    }
    const std::size_t slash = value.rfind('/');
    return (slash == std::string_view::npos ? std::string{}
                                            : crumbs(value.substr(0, slash)) + std::string{crumb}) +
           library::roms::presetName(value);
}

bool ShaderPresetPicker::owns(const std::string& id) {
    return id.starts_with(prefix);
}

void ShaderPresetPicker::open() {
    presets_ = pack_.presets();
    folder_.clear();
    returnRow_.clear();
    open_ = true;
}

void ShaderPresetPicker::close() noexcept {
    open_ = false;
    folder_.clear();
    presets_.clear();
}

std::string ShaderPresetPicker::openName() const {
    return folder_.empty() ? std::string{"Shader preset"}
                           : "Shader preset" + std::string{crumb} + crumbs(folder_);
}

std::vector<ui::SettingsRow> ShaderPresetPicker::rows(const std::string* own,
                                                      const std::string& inherited) const {
    std::vector<ui::SettingsRow> rows;
    if (folder_.empty()) {
        rows.push_back(ui::makeRow(std::string{clearId}, RowKind::Action, "Not set",
                                   "The layer below decides: " + presetLabel(inherited),
                                   own == nullptr ? "Chosen" : ""));
        rows.push_back(ui::makeRow(std::string{noneId}, RowKind::Action, "None",
                                   "No preset, whatever the layers below say; the filter applies",
                                   own != nullptr && own->empty() ? "Chosen" : ""));
        if (presets_.empty()) {
            rows.push_back(ui::makeRow("shaders.missing", RowKind::Info, "No presets downloaded",
                                       "Download Shader presets in Settings › Emulators", ""));
            return rows;
        }
    }
    // Subfolders in pack order with the presets anywhere below each.
    std::vector<std::pair<std::string, std::size_t>> folders;
    for (const library::roms::ShaderPreset& preset : presets_) {
        std::string_view rest;
        if (preset.folder.empty() || preset.folder == folder_ ||
            !below(preset.folder, folder_, rest)) {
            continue;
        }
        const std::string child{rest.substr(0, rest.find('/'))};
        const auto at = std::ranges::find(folders, child, &decltype(folders)::value_type::first);
        if (at == folders.end()) {
            folders.emplace_back(child, 1);
        } else {
            ++at->second;
        }
    }
    for (const auto& [child, count] : folders) {
        const std::string path = folder_.empty() ? child : folder_ + "/" + child;
        std::string_view rest;
        const bool chosen = own != nullptr && below(*own, path, rest);
        rows.push_back(ui::makeRow(std::string{folderId} + path, RowKind::Action, child, "",
                                   chosen ? "Chosen: " + library::roms::presetName(*own)
                                          : presetsCount(count)));
    }
    for (const library::roms::ShaderPreset& preset : presets_) {
        if (preset.folder != folder_) {
            continue;
        }
        const bool chosen = own != nullptr && *own == preset.path;
        rows.push_back(ui::makeRow(std::string{presetId} + preset.path, RowKind::Action,
                                   preset.name, preset.path, chosen ? "Chosen" : ""));
    }
    return rows;
}

ShaderPresetPicker::Pick ShaderPresetPicker::press(const std::string& id) {
    const std::string_view row{id};
    returnRow_.clear();
    if (row == clearId) {
        return Pick{Pick::Kind::Clear, {}};
    }
    if (row == noneId) {
        return Pick{Pick::Kind::Set, {}};
    }
    if (row.starts_with(folderId)) {
        folder_ = std::string{row.substr(folderId.size())};
        return {};
    }
    if (row.starts_with(presetId)) {
        const std::string path{row.substr(presetId.size())};
        if (std::ranges::any_of(presets_, [&path](const auto& preset) {
                return preset.path == path;
            })) {
            return Pick{Pick::Kind::Set, path};
        }
    }
    return {};
}

bool ShaderPresetPicker::back() {
    if (folder_.empty()) {
        close();
        return false;
    }
    returnRow_ = std::string{folderId} + folder_;
    const std::size_t slash = folder_.rfind('/');
    folder_.resize(slash == std::string::npos ? 0 : slash);
    return true;
}

} // namespace opensu::app

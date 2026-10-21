#include "layer_settings.hpp"

#include <algorithm>

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {
namespace {

using settings::Layer;
using ui::RowKind;

constexpr std::string_view prefix = "layers.";
constexpr std::string_view openGlobalId = "layers.open.global";
constexpr std::string_view openSystemId = "layers.open.system.";
constexpr std::string_view openEmulatorId = "layers.open.emulator.";
constexpr std::string_view emulatorRowId = "layers.emulator";
constexpr std::string_view videoRowId = "layers.video.";
constexpr std::string_view shaderRowId = "layers.shader";
constexpr std::string_view clearId = "layers.clear";

/// `count` keys set, as a row's value.
std::string keptText(std::size_t count) {
    if (count == 0) {
        return "Nothing set";
    }
    return std::to_string(count) + (count == 1 ? " setting" : " settings");
}

/// Steps `from` by `step` (A is 1) around `count` places.
std::size_t stepped(std::size_t from, int step, std::size_t count) {
    const auto size = static_cast<int>(count);
    return static_cast<std::size_t>((static_cast<int>(from) + (step == 0 ? 1 : step) + size) %
                                    size);
}

/// The emulators `catalog` lists for `system`, "" (Automatic) first.
std::vector<std::string> namesFor(const library::roms::EmulatorCatalog& catalog,
                                  std::string_view system) {
    std::vector<std::string> names{""};
    for (const library::roms::EmulatorEntry& entry : catalog.entries()) {
        if (entry.serves(system)) {
            names.push_back(entry.name);
        }
    }
    return names;
}

} // namespace

bool LayerSettings::owns(const std::string& id) {
    return id.starts_with(prefix) || ShaderPresetPicker::owns(id);
}

std::string LayerSettings::nameOf(const Layer& layer) const {
    return layerName(layer, found_().catalog());
}

std::vector<ui::SettingsRow> LayerSettings::openRows() const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    const auto kept = [&layers](const Layer& layer) {
        const auto found = layers.layers().find(layer);
        return keptText(found == layers.layers().end() ? 0 : found->second.size());
    };
    std::vector<ui::SettingsRow> rows;
    rows.push_back(ui::makeRow(std::string{openGlobalId}, RowKind::Action, "Global",
                               "Video for every game on a built-in core", kept(Layer{})));
    const library::roms::EmulatorCatalog& catalog = found_().catalog();
    for (const std::string& system : catalog.servedSystems()) {
        const std::string label = library::roms::systemLabel(system);
        rows.push_back(ui::makeRow(std::string{openSystemId} + system, RowKind::Action, label,
                                   "Emulator and video for " + label + " games",
                                   kept(Layer{Layer::Kind::System, system})));
    }
    for (const library::roms::EmulatorEntry& entry : catalog.entries()) {
        if (entry.builtIn()) {
            rows.push_back(ui::makeRow(std::string{openEmulatorId} + entry.id, RowKind::Action,
                                       entry.name, "Video for every game it runs, on any console",
                                       kept(Layer{Layer::Kind::Emulator, entry.id})));
        }
    }
    return rows;
}

ui::SettingsRow LayerSettings::emulatorRow(const std::string& system) const {
    const std::string* pick =
        gameSettings_.layers().at(Layer{Layer::Kind::System, system}, library::emulatorKey);
    const std::string automatic = found_().chosenName(system);
    if (pick != nullptr) {
        return ui::makeRow(std::string{emulatorRowId}, RowKind::Choice, "Emulator",
                           "Set for " + library::roms::systemLabel(system) +
                               "; runs games that have no emulator of their own",
                           *pick);
    }
    return ui::makeRow(std::string{emulatorRowId}, RowKind::Choice, "Emulator",
                       "The emulator found, or the core an arcade romset matches",
                       "Automatic" + (automatic.empty() ? std::string{} : " (" + automatic + ")"));
}

ui::SettingsRow LayerSettings::videoRow(const Layer& layer, const settings::VideoKey& video) const {
    ui::SettingsRow row = videoRowOf(layer, video);
    if (settings::replacedByShader(video.key)) {
        const std::string* own = gameSettings_.layers().at(layer, settings::shaderKey);
        if (!(own != nullptr ? *own : inheritedShader(layer)).empty()) {
            row.note += " · not used with a shader preset";
        }
    }
    return row;
}

ui::SettingsRow LayerSettings::videoRowOf(const Layer& layer,
                                          const settings::VideoKey& video) const {
    const settings::SettingLayers& layers = gameSettings_.layers();
    const std::string id = std::string{videoRowId} + std::string{video.key};
    if (const std::string* own = layers.at(layer, video.key)) {
        return ui::makeRow(id, RowKind::Choice, std::string{video.label},
                           "Set for " + nameOf(layer),
                           std::string{settings::choiceLabel(video, *own)});
    }
    const std::string* everywhere = layers.at(Layer{}, video.key);
    const std::string_view inherited =
        everywhere != nullptr ? settings::choiceLabel(video, *everywhere) : video.choices[0].label;
    // Under a core come each game's console, then Global.
    std::string note = "Default";
    if (layer.kind == Layer::Kind::Emulator) {
        note = "Not set; from each game's console";
    } else if (layer.kind == Layer::Kind::System && everywhere != nullptr) {
        note = "From Global";
    }
    return ui::makeRow(id, RowKind::Choice, std::string{video.label}, note, std::string{inherited});
}

std::string LayerSettings::inheritedShader(const Layer& layer) const {
    const std::string* everywhere = gameSettings_.layers().at(Layer{}, settings::shaderKey);
    return layer.kind == Layer::Kind::System && everywhere != nullptr ? *everywhere : std::string{};
}

ui::SettingsRow LayerSettings::shaderRow(const Layer& layer) const {
    const std::string id{shaderRowId};
    const std::string label{settings::shaderLabel};
    if (const std::string* own = gameSettings_.layers().at(layer, settings::shaderKey)) {
        return ui::makeRow(id, RowKind::Action, label,
                           "Set for " + nameOf(layer) + "; replaces Sampling and Filter",
                           presetLabel(*own));
    }
    std::string note =
        picker_.packInstalled() ? "Default" : "Default; Download Shader presets to choose one";
    if (layer.kind == Layer::Kind::Emulator) {
        note = "Not set; from each game's console";
    } else if (layer.kind == Layer::Kind::System &&
               gameSettings_.layers().at(Layer{}, settings::shaderKey) != nullptr) {
        note = "From Global";
    }
    return ui::makeRow(id, RowKind::Action, label, note, presetLabel(inheritedShader(layer)));
}

std::string LayerSettings::openName() const {
    if (!open_) {
        return {};
    }
    return picker_.isOpen() ? nameOf(*open_) + " › " + picker_.openName() : nameOf(*open_);
}

LayerSettings::Back LayerSettings::back() {
    leftPicker_ = false;
    if (!picker_.isOpen()) {
        return Back::Layer;
    }
    if (picker_.back()) {
        return Back::Within;
    }
    leftPicker_ = true;
    return Back::Within;
}

std::string LayerSettings::returnRow() const {
    if (picker_.isOpen()) {
        return picker_.returnRow();
    }
    return leftPicker_ ? std::string{shaderRowId} : std::string{};
}

void LayerSettings::pressPicker(const Layer& layer, const std::string& id) {
    const ShaderPresetPicker::Pick pick = picker_.press(id);
    using Kind = ShaderPresetPicker::Pick::Kind;
    if (pick.kind == Kind::Nothing) {
        return;
    }
    if (pick.kind == Kind::Clear) {
        gameSettings_.layers().clear(layer, settings::shaderKey);
    } else {
        gameSettings_.layers().set(layer, settings::shaderKey, pick.value);
    }
    saveVideo();
    picker_.close();
    leftPicker_ = true;
}

void LayerSettings::saveVideo() {
    gameSettings_.save();
    hooks_.videoChanged();
}

std::optional<ui::SettingsCategory> LayerSettings::page(const std::string& id,
                                                        const std::string& label) const {
    if (!open_) {
        return std::nullopt;
    }
    ui::SettingsCategory page{id, label, {}};
    if (picker_.isOpen()) {
        page.rows = picker_.rows(gameSettings_.layers().at(*open_, settings::shaderKey),
                                 inheritedShader(*open_));
        return page;
    }
    if (open_->kind == Layer::Kind::System) {
        page.rows.push_back(emulatorRow(open_->id));
    }
    for (const settings::VideoKey& video : settings::videoKeys) {
        page.rows.push_back(videoRow(*open_, video));
    }
    page.rows.push_back(shaderRow(*open_));
    page.rows.push_back(ui::makeRow(std::string{clearId}, RowKind::Action,
                                    "Clear " + nameOf(*open_) + " settings",
                                    "Every value here goes back to the layer under it", "Clear"));
    return page;
}

void LayerSettings::run(const std::string& id, int step) {
    const std::string_view row{id};
    if (row == openGlobalId || row.starts_with(openSystemId) || row.starts_with(openEmulatorId)) {
        if (step != 0) {
            return;
        }
        if (row == openGlobalId) {
            open_ = Layer{};
        } else if (row.starts_with(openSystemId)) {
            open_ = Layer{Layer::Kind::System, std::string{row.substr(openSystemId.size())}};
        } else {
            open_ = Layer{Layer::Kind::Emulator, std::string{row.substr(openEmulatorId.size())}};
        }
        return;
    }
    if (!open_) {
        return;
    }
    leftPicker_ = false;
    const Layer layer = *open_;
    if (picker_.isOpen()) {
        if (ShaderPresetPicker::owns(id) && step == 0) {
            pressPicker(layer, id);
        }
        return;
    }
    if (row == shaderRowId && step == 0) {
        picker_.open();
    } else if (row == emulatorRowId && layer.kind == Layer::Kind::System) {
        stepEmulator(layer.id, step);
    } else if (row.starts_with(videoRowId)) {
        stepVideo(layer, row.substr(videoRowId.size()), step);
    } else if (row == clearId && step == 0) {
        clearLayer(layer);
    }
}

void LayerSettings::stepEmulator(const std::string& system, int step) {
    const std::vector<std::string> names = namesFor(found_().catalog(), system);
    const Layer layer{Layer::Kind::System, system};
    settings::SettingLayers& layers = gameSettings_.layers();
    const std::string* pick = layers.at(layer, library::emulatorKey);
    const auto at = std::ranges::find(names, pick == nullptr ? std::string{} : *pick);
    const std::size_t from = at == names.end() ? 0 : static_cast<std::size_t>(at - names.begin());
    const std::string& next = names[stepped(from, step, names.size())];
    if (next.empty()) {
        layers.clear(layer, library::emulatorKey);
    } else {
        layers.set(layer, library::emulatorKey, next);
    }
    gameSettings_.save();
    hooks_.picksChanged();
}

void LayerSettings::stepVideo(const Layer& layer, std::string_view key, int step) {
    const settings::VideoKey* video = settings::videoKey(key);
    if (video == nullptr) {
        return;
    }
    settings::SettingLayers& layers = gameSettings_.layers();
    // Place 0 is "not set"; place n + 1 is choice n.
    std::size_t from = 0;
    if (const std::string* own = layers.at(layer, key)) {
        from = settings::choiceIndex(*video, *own).value_or(0) + 1;
    }
    const std::size_t next = stepped(from, step, video->choices.size() + 1);
    if (next == 0) {
        layers.clear(layer, key);
    } else {
        layers.set(layer, key, std::string{video->choices[next - 1].key});
    }
    saveVideo();
}

void LayerSettings::clearLayer(const Layer& layer) {
    settings::SettingLayers& layers = gameSettings_.layers();
    const bool picked = layers.at(layer, library::emulatorKey) != nullptr;
    layers.clear(layer, library::emulatorKey);
    for (const settings::VideoKey& video : settings::videoKeys) {
        layers.clear(layer, video.key);
    }
    layers.clear(layer, settings::shaderKey);
    saveVideo();
    if (picked) {
        hooks_.picksChanged();
    }
}

std::string LayerSettings::openRowId() const {
    if (!open_ || open_->kind == Layer::Kind::Global) {
        return std::string{openGlobalId};
    }
    return std::string{open_->kind == Layer::Kind::System ? openSystemId : openEmulatorId} +
           open_->id;
}

std::size_t LayerSettings::gamePicks() const {
    return static_cast<std::size_t>(
        std::ranges::count_if(gameSettings_.layers().layers(), [](const auto& kept) {
            return kept.first.kind == Layer::Kind::Game &&
                   kept.second.contains(library::emulatorKey);
        }));
}

void LayerSettings::clearGamePicks() {
    std::vector<Layer> picked;
    for (const auto& [layer, values] : gameSettings_.layers().layers()) {
        if (layer.kind == Layer::Kind::Game && values.contains(library::emulatorKey)) {
            picked.push_back(layer);
        }
    }
    for (const Layer& layer : picked) {
        gameSettings_.layers().clear(layer, library::emulatorKey);
    }
    gameSettings_.save();
}

} // namespace opensu::app

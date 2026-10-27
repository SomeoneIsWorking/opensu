#include "layer_settings.hpp"

#include <algorithm>

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"

namespace opensu::app {
namespace {

using settings::Layer;

constexpr std::string_view prefix = "layers.";
constexpr std::string_view openGlobalId = "layers.open.global";
constexpr std::string_view openSystemId = "layers.open.system.";
constexpr std::string_view openEmulatorId = "layers.open.emulator.";
constexpr std::string_view consolesId = "layers.list.consoles";
constexpr std::string_view coresId = "layers.list.cores";

} // namespace

bool LayerSettings::owns(const std::string& id) {
    return id.starts_with(prefix);
}

std::string LayerSettings::keptText(const EditorContext& context) const {
    const std::size_t count = layered_.kept(context);
    if (count == 0) {
        return "Nothing set";
    }
    return std::to_string(count) + (count == 1 ? " setting" : " settings");
}

std::vector<ui::SettingsRow> LayerSettings::pageRows() const {
    const library::roms::EmulatorCatalog& catalog = found_().catalog();
    const auto cores =
        std::ranges::count_if(catalog.entries(), &library::roms::EmulatorEntry::builtIn);
    return {
        ui::makePage(std::string{openGlobalId}, "Global",
                     "Video and frame generation for every game", keptText(layered_.forGlobal())),
        ui::makePage(std::string{consolesId}, "Consoles", "Emulator and video for each console",
                     std::to_string(catalog.servedSystems().size())),
        ui::makePage(std::string{coresId}, "Built-in cores",
                     "Video and core options for every game a core runs", std::to_string(cores)),
    };
}

std::optional<ui::SettingsCategory> LayerSettings::listPage(std::string_view id) const {
    if (id != consolesId && id != coresId) {
        return std::nullopt;
    }
    const library::roms::EmulatorCatalog& catalog = found_().catalog();
    if (id == consolesId) {
        ui::SettingsCategory page{std::string{id}, "Consoles", {}};
        for (const std::string& system : catalog.servedSystems()) {
            const std::string label = library::roms::systemLabel(system);
            page.rows.push_back(ui::makePage(std::string{openSystemId} + system, label,
                                             "Emulator and video for " + label + " games",
                                             keptText(layered_.forSystem(system))));
        }
        return page;
    }
    ui::SettingsCategory page{std::string{id}, "Built-in cores", {}};
    for (const library::roms::EmulatorEntry& entry : catalog.entries()) {
        if (entry.builtIn()) {
            page.rows.push_back(ui::makePage(std::string{openEmulatorId} + entry.id,
                                             std::string{entry.shortName()},
                                             "Video and core options for every game it runs",
                                             keptText(layered_.forCore(entry.id))));
        }
    }
    return page;
}

std::optional<EditorContext> LayerSettings::contextOf(std::string_view id) const {
    if (id == openGlobalId) {
        return layered_.forGlobal();
    }
    if (id.starts_with(openSystemId)) {
        return layered_.forSystem(std::string{id.substr(openSystemId.size())});
    }
    if (id.starts_with(openEmulatorId)) {
        return layered_.forCore(std::string{id.substr(openEmulatorId.size())});
    }
    return std::nullopt;
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

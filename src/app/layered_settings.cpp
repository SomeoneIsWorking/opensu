#include "layered_settings.hpp"

#include <algorithm>

#include "library/emulator_choice.hpp"
#include "library/rom_systems.hpp"
#include "retro_launch.hpp"
#include "settings/core_option_settings.hpp"
#include "settings/framegen_settings.hpp"
#include "settings/video_settings.hpp"

namespace opensu::app {
namespace {

using settings::Layer;

/// Whether `entry` is a built-in core.
bool builtIn(const library::roms::EmulatorCatalog& catalog, const std::string& id) {
    const library::roms::EmulatorEntry* entry = catalog.find(id);
    return entry != nullptr && entry->builtIn();
}

} // namespace

LayeredSettings::LayeredSettings(Sources sources, Hooks hooks)
    : gameSettings_{sources.gameSettings}, found_{std::move(sources.found)},
      shaders_{sources.shaders}, frameGeneration_{sources.frameGeneration}, retro_{sources.retro},
      coreOptions_{std::move(sources.coreOptions)}, hooks_{std::move(hooks)} {
}

EditorContext LayeredSettings::forGame(const library::Game& game) const {
    EditorContext context{library::layerKeysOf(game), {Layer{}}, game.id};
    if (!context.keys.system.empty()) {
        context.scopes.push_back(Layer{Layer::Kind::System, context.keys.system});
    }
    if (!context.keys.emulator.empty() && builtIn(found_().catalog(), context.keys.emulator)) {
        context.scopes.push_back(Layer{Layer::Kind::Emulator, context.keys.emulator});
    }
    context.scopes.push_back(Layer{Layer::Kind::Game, game.id});
    return context;
}

EditorContext LayeredSettings::forGlobal() const {
    return EditorContext{{}, {Layer{}}, {}};
}

EditorContext LayeredSettings::forSystem(const std::string& system) const {
    return EditorContext{
        settings::LayerKeys{.system = system}, {Layer{}, Layer{Layer::Kind::System, system}}, {}};
}

EditorContext LayeredSettings::forCore(const std::string& core) const {
    return EditorContext{
        settings::LayerKeys{.emulator = core}, {Layer{}, Layer{Layer::Kind::Emulator, core}}, {}};
}

std::optional<library::Game> LayeredSettings::gameOf(const EditorContext& context) const {
    if (context.gameId.empty()) {
        return std::nullopt;
    }
    if (const library::Game* running = hooks_.running();
        running != nullptr && running->id == context.gameId) {
        return *running;
    }
    return hooks_.game(context.gameId);
}

std::vector<settings::CoreOptionDefinition>
LayeredSettings::coreOptionsOf(const std::string& core) const {
    const library::Game* running = hooks_.running();
    if (running != nullptr && library::layerKeysOf(*running).emulator == core && retro_.status()) {
        std::vector<settings::CoreOptionDefinition> options;
        for (RetroSession::CoreOption& option : retro_.options()) {
            options.push_back(std::move(option.definition));
        }
        if (!options.empty()) {
            auto& kept = coreOptionsKnown_[core];
            if (kept != options) {
                coreOptions_.remember(core, options);
                kept = options;
            }
            return options;
        }
    }
    const auto kept = coreOptionsKnown_.find(core);
    if (kept != coreOptionsKnown_.end()) {
        return kept->second;
    }
    return coreOptionsKnown_.emplace(core, coreOptions_.known(core)).first->second;
}

std::string LayeredSettings::automaticLabel(const std::string& system) const {
    const std::string found = found_().chosenName(system);
    if (found.empty()) {
        return "Automatic";
    }
    const library::roms::EmulatorEntry* entry = found_().catalog().findByName(found);
    return "Automatic: " + (entry != nullptr ? std::string{entry->shortName()} : found);
}

std::optional<settings::Setting>
LayeredSettings::emulatorPickOf(const EditorContext& context) const {
    if (const std::optional<library::Game> game = gameOf(context)) {
        if (game->source != library::Source::Rom || game->emulatorOptions.size() < 2) {
            return std::nullopt;
        }
        std::vector<std::string> names;
        for (const library::EmulatorOption& option : game->emulatorOptions) {
            names.push_back(option.name);
        }
        return library::emulatorPickSetting(names, automaticLabel(game->sourceId));
    }
    if (context.keys.system.empty()) {
        return std::nullopt;
    }
    std::vector<std::string> names;
    for (const library::roms::EmulatorEntry& entry : found_().catalog().entries()) {
        if (entry.serves(context.keys.system)) {
            names.push_back(entry.name);
        }
    }
    return library::emulatorPickSetting(names, automaticLabel(context.keys.system));
}

settings::Schema LayeredSettings::schemaOf(const EditorContext& context) const {
    settings::Schema schema;
    const library::roms::EmulatorCatalog& catalog = found_().catalog();
    const std::optional<library::Game> game = gameOf(context);
    const bool video =
        game ? builtInOf(*game, catalog) != nullptr
             : context.keys.emulator.empty() || builtIn(catalog, context.keys.emulator);
    if (video) {
        settings::declareVideo(schema);
    }
    if (FrameGeneration::supported()) {
        settings::declareFrameGen(schema);
    }
    if (video && !context.keys.emulator.empty()) {
        settings::declareCoreOptions(schema, coreOptionsOf(context.keys.emulator));
    }
    // Last: it is changed least, and only from the next launch.
    if (std::optional<settings::Setting> pick = emulatorPickOf(context)) {
        schema.add(std::move(*pick));
    }
    return schema;
}

std::vector<settings::ScopedValue> LayeredSettings::rows(const settings::Schema& schema,
                                                         const EditorContext& context,
                                                         const Layer& scope) const {
    return settings::scopedValues(schema, gameSettings_.layers(), context.keys, scope);
}

bool LayeredSettings::offers(const library::Game& game) const {
    const EditorContext context = forGame(game);
    return !rows(schemaOf(context), context, context.scopes.back()).empty();
}

std::string LayeredSettings::sectionsOf(const library::Game& game) const {
    const EditorContext context = forGame(game);
    const settings::Schema schema = schemaOf(context);
    std::string joined;
    std::string last;
    for (const settings::ScopedValue& row : rows(schema, context, context.scopes.back())) {
        if (row.setting->section != last) {
            last = row.setting->section;
            joined += (joined.empty() ? "" : ", ") + last;
        }
    }
    return joined;
}

std::size_t LayeredSettings::kept(const library::Game& game) const {
    return kept(forGame(game));
}

std::size_t LayeredSettings::kept(const EditorContext& context) const {
    return settings::keptCount(rows(schemaOf(context), context, context.scopes.back()));
}

std::string LayeredSettings::scopeName(const Layer& scope) const {
    return layerName(scope, found_().catalog());
}

std::string LayeredSettings::frameGenSource() const {
    std::string text = frameGeneration_.dll()
                           ? "Lossless Scaling's shaders, from your Steam install"
                           : "Needs Lossless Scaling, installed through Steam";
    if (!frameGeneration_.active()) {
        text += " · applies in openSU's Gamescope session";
    }
    return text;
}

void LayeredSettings::set(const EditorContext& context, const Layer& scope, const std::string& key,
                          const std::string& value) {
    settings::SettingLayers& layers = gameSettings_.layers();
    // Custom starts from what the preset it replaces did.
    if (key == settings::frameGenKeys.front().key && value == "custom") {
        const std::string before =
            layers
                .resolve(context.keys, key,
                         std::string{settings::frameGenKeys.front().defaultKey()})
                .value;
        for (const auto& [detail, seeded] : settings::frameGenPresetValues(before)) {
            if (layers.at(scope, detail) == nullptr) {
                layers.set(scope, detail, std::string{seeded});
            }
        }
    }
    layers.set(scope, key, value);
    removed_.erase({scope, key});
    changed(key);
}

void LayeredSettings::remove(const Layer& scope, const std::string& key) {
    settings::SettingLayers& layers = gameSettings_.layers();
    const std::string* own = layers.at(scope, key);
    if (own == nullptr) {
        return;
    }
    removed_[{scope, key}] = *own;
    layers.clear(scope, key);
    changed(key);
}

const std::string* LayeredSettings::removed(const Layer& scope, const std::string& key) const {
    const auto found = removed_.find({scope, key});
    return found == removed_.end() ? nullptr : &found->second;
}

void LayeredSettings::restore(const Layer& scope, const std::string& key) {
    const auto found = removed_.find({scope, key});
    if (found == removed_.end()) {
        return;
    }
    gameSettings_.layers().set(scope, key, found->second);
    removed_.erase(found);
    changed(key);
}

std::size_t LayeredSettings::clear(const Layer& scope) {
    const bool picked = gameSettings_.layers().at(scope, library::emulatorKey) != nullptr;
    const std::size_t cleared = gameSettings_.clear(scope);
    changed(picked ? std::string{library::emulatorKey} : std::string{});
    return cleared;
}

void LayeredSettings::changed(const std::string& key) {
    gameSettings_.save();
    if (key == library::emulatorKey) {
        hooks_.picksChanged();
    }
    applyToRunning();
    frameGeneration_.changed();
    hooks_.changed();
}

void LayeredSettings::applyToRunning() {
    const library::Game* running = hooks_.running();
    if (running == nullptr || !retro_.status()) {
        return;
    }
    const settings::SettingLayers& layers = gameSettings_.layers();
    if (const std::optional<settings::VideoSettings> video =
            retroVideoOf(*running, layers, found_().catalog(), shaders_.root())) {
        if (const std::string refused = retro_.setVideo(*video); !refused.empty()) {
            hooks_.say(refused, true);
        }
    }
    const std::map<std::string, std::string> wanted =
        settings::resolveCoreOptions(layers, library::layerKeysOf(*running));
    for (const RetroSession::CoreOption& option : retro_.options()) {
        const auto kept = wanted.find(option.definition.key);
        const std::string& value = kept != wanted.end() ? kept->second : option.definition.fallback;
        if (value == option.value) {
            continue;
        }
        if (const std::string refused = retro_.setOption(option.definition.key, value);
            !refused.empty()) {
            hooks_.say(refused, true);
        }
    }
}

} // namespace opensu::app

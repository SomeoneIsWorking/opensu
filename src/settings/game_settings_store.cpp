#include "game_settings_store.hpp"

#include <array>
#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::settings {
namespace {

using json = nlohmann::json;

constexpr const char* globalKey = "global";

/// The section each kind of layer but the global one is kept in, by id.
struct Section {
    Layer::Kind kind;
    const char* name;
};
constexpr std::array<Section, 3> sections{{
    {Layer::Kind::System, "systems"},
    {Layer::Kind::Emulator, "emulators"},
    {Layer::Kind::Game, "games"},
}};

/// `values`' text entries into `layer`; anything else is reported and left out.
void readLayer(const json& values, const Layer& layer, SettingLayers& into,
               const std::string& file) {
    if (!values.is_object()) {
        lucent::warn("settings", "{}: {} is not a layer's settings; left out", file, values.dump());
        return;
    }
    for (const auto& [key, value] : values.items()) {
        if (value.is_string()) {
            into.set(layer, key, value.get<std::string>());
        } else {
            lucent::warn("settings", "{}: {} {} is not text; left out", file, key, value.dump());
        }
    }
}

} // namespace

SettingLayers GameSettingsStore::load() const {
    SettingLayers layers;
    std::ifstream in{file_, std::ios::binary};
    if (!in) {
        return layers;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const json document = json::parse(text, nullptr, false);
    if (!document.is_object()) {
        lucent::warn("settings", "{} is not a game settings file; using none", file_.string());
        return layers;
    }
    if (const auto global = document.find(globalKey); global != document.end()) {
        readLayer(*global, Layer{}, layers, file_.string());
    }
    for (const Section& section : sections) {
        const auto found = document.find(section.name);
        if (found == document.end()) {
            continue;
        }
        if (!found->is_object()) {
            lucent::warn("settings", "{}: {} is not a list of layers; left out", file_.string(),
                         section.name);
            continue;
        }
        for (const auto& [id, values] : found->items()) {
            readLayer(values, Layer{section.kind, id}, layers, file_.string());
        }
    }
    return layers;
}

bool GameSettingsStore::save(const SettingLayers& layers, std::string& error) const {
    json document = json::object();
    for (const auto& [layer, values] : layers.layers()) {
        json kept = json::object();
        for (const auto& [key, value] : values) {
            kept[key] = value;
        }
        if (layer.kind == Layer::Kind::Global) {
            document[globalKey] = kept;
            continue;
        }
        for (const Section& section : sections) {
            if (section.kind == layer.kind) {
                document[section.name][layer.id] = kept;
            }
        }
    }
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace opensu::settings

#include "mapping_store.hpp"

#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <variant>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::settings {
namespace {

using json = nlohmann::json;
using gamepad::Button;

constexpr const char* globalKey = "global";
constexpr const char* systemsKey = "systems";
constexpr const char* emulatorsKey = "emulators";
constexpr const char* gamesKey = "games";

/// The section each kind of layer but the global one keeps its mappings in, by id.
struct Section {
    const char* key;
    Layer::Kind kind;
};
constexpr std::array<Section, 3> sections{{
    {systemsKey, Layer::Kind::System},
    {emulatorsKey, Layer::Kind::Emulator},
    {gamesKey, Layer::Kind::Game},
}};
constexpr const char* steamInputKey = "steamInput";
constexpr const char* buttonsKey = "buttons";
constexpr const char* chordsKey = "chords";
constexpr const char* modifierKey = "modifier";
constexpr const char* triggerKey = "trigger";
constexpr const char* actionKey = "action";

json buttonsJson(const input::PadOutput& output) {
    json list = json::array();
    for (const Button button : output) {
        list.push_back(std::string{gamepad::name(button)});
    }
    return list;
}

json mapJson(const input::ButtonMap& map) {
    json buttons = json::object();
    for (const auto& [button, output] : map.buttons()) {
        buttons[std::string{gamepad::name(button)}] = buttonsJson(output);
    }
    json chords = json::array();
    for (const input::ChordMapping& mapping : map.chords()) {
        json chord{{modifierKey, std::string{gamepad::name(mapping.chord.modifier)}},
                   {triggerKey, std::string{gamepad::name(mapping.chord.trigger)}}};
        if (const auto* action = std::get_if<input::Action>(&mapping.output)) {
            chord[actionKey] = std::string{input::spelling(*action)};
        } else {
            chord[buttonsKey] = buttonsJson(std::get<input::PadOutput>(mapping.output));
        }
        chords.push_back(std::move(chord));
    }
    return json{{buttonsKey, std::move(buttons)}, {chordsKey, std::move(chords)}};
}

/// Reads one mapping, each entry on its own: a damaged one is reported and left out.
class MapReader {
  public:
    MapReader(std::string file, std::string where)
        : file_{std::move(file)}, where_{std::move(where)} {
    }

    [[nodiscard]] std::optional<input::ButtonMap> read(const json& value) const {
        if (!value.is_object()) {
            warn("not a mapping");
            return std::nullopt;
        }
        input::ButtonMap map;
        const json given = value.value(buttonsKey, json::object());
        if (!given.is_object()) {
            warn("buttons not an object");
        }
        const json buttons = given.is_object() ? given : json::object();
        for (const auto& [name, output] : buttons.items()) {
            const Button button = gamepad::buttonNamed(name);
            const std::optional<input::PadOutput> sends = padOutput(output);
            if (button == Button::None || !sends) {
                warn("button " + name +
                     (button == Button::None ? " is not a button"
                                             : " does not send a list of buttons"));
                continue;
            }
            keep(map.setButton(button, *sends), "button " + name);
        }
        const json chords = value.value(chordsKey, json::array());
        for (const json& chord : chords.is_array() ? chords : json::array()) {
            readChord(chord, map);
        }
        return map;
    }

  private:
    [[nodiscard]] static std::optional<input::PadOutput> padOutput(const json& value) {
        if (!value.is_array()) {
            return std::nullopt;
        }
        input::PadOutput output;
        for (const json& item : value) {
            const Button button =
                item.is_string() ? gamepad::buttonNamed(item.get<std::string>()) : Button::None;
            if (button == Button::None) {
                return std::nullopt;
            }
            output.insert(button);
        }
        return output;
    }

    void readChord(const json& chord, input::ButtonMap& map) const {
        const auto named = [&chord](const char* key) {
            const auto found = chord.find(key);
            return found != chord.end() && found->is_string()
                       ? gamepad::buttonNamed(found->get<std::string>())
                       : Button::None;
        };
        if (!chord.is_object()) {
            warn("a chord is not an object");
            return;
        }
        const input::PadChord pair{named(modifierKey), named(triggerKey)};
        const std::string what = "chord " + input::describe(pair);
        if (const auto action = chord.find(actionKey); action != chord.end()) {
            const std::optional<input::Action> performs =
                action->is_string() ? input::actionOf(action->get<std::string>()) : std::nullopt;
            if (!performs) {
                warn(what + " has no such action");
                return;
            }
            keep(map.setChord(pair, *performs), what);
            return;
        }
        const auto buttons = chord.find(buttonsKey);
        const std::optional<input::PadOutput> sends =
            buttons != chord.end() ? padOutput(*buttons) : std::nullopt;
        if (!sends) {
            warn(what + " needs a list of buttons or an action");
            return;
        }
        keep(map.setChord(pair, *sends), what);
    }

    void keep(const std::string& refused, const std::string& what) const {
        if (!refused.empty()) {
            warn(what + ": " + refused);
        }
    }

    void warn(const std::string& why) const {
        lucent::warn("settings", "{}: {}: {}; left out", file_, where_, why);
    }

    std::string file_;
    std::string where_;
};

} // namespace

input::MappingScopes MappingStore::load() const {
    input::MappingScopes scopes;
    std::ifstream in{file_, std::ios::binary};
    if (!in) {
        return scopes;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const json document = json::parse(text, nullptr, false);
    const std::string file = file_.string();
    if (!document.is_object()) {
        lucent::warn("settings", "{} is not a mappings file; no mapping applies", file);
        return scopes;
    }
    if (const auto global = document.find(globalKey); global != document.end()) {
        if (std::optional<input::ButtonMap> map = MapReader{file, globalKey}.read(*global)) {
            scopes.set(Layer{}, std::move(*map));
        }
    }
    for (const auto& [key, kind] : sections) {
        const json entries = document.value(key, json::object());
        if (!entries.is_object()) {
            lucent::warn("settings", "{}: {} is not an object; left out", file, key);
            continue;
        }
        for (const auto& [id, value] : entries.items()) {
            if (std::optional<input::ButtonMap> map =
                    MapReader{file, std::string{key} + "." + id}.read(value)) {
                scopes.set(Layer{kind, id}, std::move(*map));
            }
        }
    }
    const json steam = document.value(steamInputKey, json::object());
    if (steam.is_object()) {
        if (const auto global = steam.find(globalKey); global != steam.end()) {
            if (global->is_boolean()) {
                scopes.setSteamInputGlobal(global->get<bool>());
            } else {
                lucent::warn("settings", "{}: steamInput.global is not true or false", file);
            }
        }
        const json games = steam.value(gamesKey, json::object());
        for (const auto& [id, on] : games.items()) {
            if (on.is_boolean()) {
                scopes.setSteamInput(id, on.get<bool>());
            } else {
                lucent::warn("settings", "{}: steamInput for {} is not true or false", file, id);
            }
        }
    } else {
        lucent::warn("settings", "{}: steamInput is not an object; Steam Input stays off", file);
    }
    return scopes;
}

bool MappingStore::save(const input::MappingScopes& scopes, std::string& error) const {
    json document = json::object();
    for (const Section& section : sections) {
        document[section.key] = json::object();
    }
    for (const auto& [layer, map] : scopes.maps()) {
        if (layer.kind == Layer::Kind::Global) {
            document[globalKey] = mapJson(map);
            continue;
        }
        for (const Section& section : sections) {
            if (section.kind == layer.kind) {
                document[section.key][layer.id] = mapJson(map);
            }
        }
    }
    json steamGames = json::object();
    for (const auto& [id, on] : scopes.steamInputGames()) {
        steamGames[id] = on;
    }
    document[steamInputKey] =
        json{{globalKey, scopes.steamInputGlobal()}, {gamesKey, std::move(steamGames)}};
    return fileio::writeWhole(file_, document.dump(2) + "\n", error);
}

} // namespace opensu::settings

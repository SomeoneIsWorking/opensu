#include "emulator_pick_migration.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "library/emulator_choice.hpp"
#include "lucent/log.h"

namespace opensu::settings {
namespace {

using json = nlohmann::json;

struct OldPicks {
    const char* key;
    Layer::Kind kind;
};
constexpr std::array<OldPicks, 2> oldPicks{{
    {"emulators", Layer::Kind::Game},
    {"systemEmulators", Layer::Kind::System},
}};

} // namespace

bool migrateEmulatorPicks(const std::filesystem::path& settingsFile, SettingLayers& layers,
                          const GameSettingsStore& store, MovedPicks& moved, std::string& error) {
    std::ifstream in{settingsFile, std::ios::binary};
    if (!in) {
        return true;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    json document = json::parse(text, nullptr, false);
    if (!document.is_object() || std::ranges::none_of(oldPicks, [&document](const OldPicks& old) {
            return document.contains(old.key);
        })) {
        return true;
    }
    for (const OldPicks& old : oldPicks) {
        const auto found = document.find(old.key);
        if (found == document.end() || !found->is_object()) {
            continue;
        }
        for (const auto& [id, name] : found->items()) {
            const Layer layer{old.kind, id};
            if (!name.is_string() || layers.at(layer, library::emulatorKey) != nullptr) {
                continue;
            }
            layers.set(layer, library::emulatorKey, name.get<std::string>());
            ++(old.kind == Layer::Kind::Game ? moved.games : moved.systems);
        }
    }
    if (!store.save(layers, error)) {
        return false;
    }
    for (const OldPicks& old : oldPicks) {
        document.erase(old.key);
    }
    if (!fileio::writeWhole(settingsFile, document.dump(2) + "\n", error)) {
        return false;
    }
    lucent::info("settings", "moved {} game and {} system emulator picks from {} to {}",
                 moved.games, moved.systems, settingsFile.string(), store.file().string());
    return true;
}

} // namespace opensu::settings

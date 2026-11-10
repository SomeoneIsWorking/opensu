#include "wii_controls.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <utility>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

constexpr std::string_view controlsFile = "wii-controls.json";
/// A game is listed by its full disc id or the three characters every region shares.
constexpr std::size_t gameCodeLength = 3;

struct Named {
    std::string_view name;
    std::uint8_t value;
};

constexpr std::array extensions{
    Named{"none", static_cast<std::uint8_t>(WiiExtension::None)},
    Named{"nunchuk", static_cast<std::uint8_t>(WiiExtension::Nunchuk)},
    Named{"classic", static_cast<std::uint8_t>(WiiExtension::Classic)},
};

constexpr std::array orientations{
    Named{"pointing", static_cast<std::uint8_t>(WiiOrientation::Pointing)},
    Named{"sideways", static_cast<std::uint8_t>(WiiOrientation::Sideways)},
    Named{"upright", static_cast<std::uint8_t>(WiiOrientation::Upright)},
};

constexpr std::array remoteKeys{std::string_view{"extension"}, std::string_view{"orientation"},
                                std::string_view{"pointer"}, std::string_view{"shake"},
                                std::string_view{"inputs"}};
constexpr std::array gameKeys{std::string_view{"title"}, std::string_view{"layout"},
                              std::string_view{"proof"}};

template <std::size_t N>
bool namedOf(const json& value, const std::array<Named, N>& names, std::uint8_t& out) {
    if (!value.is_string()) {
        return false;
    }
    const auto found = std::ranges::find(names, value.get<std::string>(), &Named::name);
    if (found == names.end()) {
        return false;
    }
    out = found->value;
    return true;
}

bool known(std::string_view key, bool game) {
    return std::ranges::find(remoteKeys, key) != remoteKeys.end() ||
           (game && std::ranges::find(gameKeys, key) != gameKeys.end());
}

/// `remote` with what `object` says of it; false with `error` naming `where` when it is malformed.
bool readRemote(const json& object, std::string_view where, bool game, WiiRemote& remote,
                std::string& error) {
    if (!object.is_object()) {
        error = std::string{where} + " is not an object";
        return false;
    }
    for (const auto& [key, value] : object.items()) {
        if (!known(key, game)) {
            error = std::string{where} + " has an unknown \"" + key + "\"";
            return false;
        }
    }
    std::uint8_t named = 0;
    if (const auto found = object.find("extension"); found != object.end()) {
        if (!namedOf(*found, extensions, named)) {
            error = std::string{where} + "'s extension is not none, nunchuk or classic";
            return false;
        }
        remote.extension = static_cast<WiiExtension>(named);
    }
    if (const auto found = object.find("orientation"); found != object.end()) {
        if (!namedOf(*found, orientations, named)) {
            error = std::string{where} + "'s orientation is not pointing, sideways or upright";
            return false;
        }
        remote.orientation = static_cast<WiiOrientation>(named);
    }
    if (const auto found = object.find("pointer"); found != object.end()) {
        if (!found->is_boolean()) {
            error = std::string{where} + "'s pointer is not true or false";
            return false;
        }
        remote.pointer = found->get<bool>();
    }
    if (const auto found = object.find("shake"); found != object.end()) {
        if (!found->is_string()) {
            error = std::string{where} + "'s shake is not an input";
            return false;
        }
        remote.shake = found->get<std::string>();
    }
    if (const auto found = object.find("inputs"); found != object.end()) {
        if (!found->is_object()) {
            error = std::string{where} + "'s inputs are not an object";
            return false;
        }
        for (const auto& [key, value] : found->items()) {
            if (!value.is_string()) {
                error = std::string{where} + "'s " + key + " is not an input";
                return false;
            }
            remote.inputs.insert_or_assign(key, value.get<std::string>());
        }
    }
    return true;
}

} // namespace

std::optional<WiiControls> WiiControls::parse(std::string_view text, std::string& error) {
    const json root = json::parse(text, nullptr, false);
    if (!root.is_object()) {
        error = "not a JSON object";
        return std::nullopt;
    }
    const auto layouts = root.find("layouts");
    const auto fallback = root.find("default");
    const auto games = root.find("games");
    if (layouts == root.end() || !layouts->is_object() || fallback == root.end() ||
        !fallback->is_string() || games == root.end() || !games->is_object()) {
        error = "it needs \"layouts\", \"default\" and \"games\"";
        return std::nullopt;
    }
    std::map<std::string, WiiRemote, std::less<>> named;
    for (const auto& [name, layout] : layouts->items()) {
        WiiRemote remote{.title = name};
        if (!readRemote(layout, "layout " + name, false, remote, error)) {
            return std::nullopt;
        }
        named.emplace(name, std::move(remote));
    }
    WiiControls controls;
    const auto chosen = named.find(fallback->get<std::string>());
    if (chosen == named.end()) {
        error = "the default layout " + fallback->get<std::string>() + " is not listed";
        return std::nullopt;
    }
    controls.fallback_ = chosen->second;
    for (const auto& [id, game] : games->items()) {
        const auto title = game.is_object() ? game.find("title") : game.end();
        const auto layout = game.is_object() ? game.find("layout") : game.end();
        if (title == game.end() || !title->is_string() || layout == game.end() ||
            !layout->is_string()) {
            error = "game " + id + " needs a title and a layout";
            return std::nullopt;
        }
        const auto base = named.find(layout->get<std::string>());
        if (base == named.end()) {
            error = "game " + id + "'s layout " + layout->get<std::string>() + " is not listed";
            return std::nullopt;
        }
        WiiRemote remote = base->second;
        remote.title = title->get<std::string>();
        if (!readRemote(game, "game " + id, true, remote, error)) {
            return std::nullopt;
        }
        controls.games_.emplace(id, std::move(remote));
    }
    return controls;
}

WiiControls WiiControls::shipped(const fs::path& assetsDir) {
    const fs::path file = assetsDir / controlsFile;
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::error("wii", "{}: cannot be read; Wii games get a bare remote", file.string());
        return {};
    }
    const std::string body{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::string error;
    std::optional<WiiControls> controls = parse(body, error);
    if (!controls) {
        lucent::error("wii", "{}: {}; Wii games get a bare remote", file.string(), error);
        return {};
    }
    return std::move(*controls);
}

WiiRemote WiiControls::remoteFor(std::string_view gameId) const {
    if (const auto game = games_.find(gameId); game != games_.end()) {
        return game->second;
    }
    if (const auto game = games_.find(gameId.substr(0, gameCodeLength));
        !gameId.empty() && game != games_.end()) {
        return game->second;
    }
    return fallback_;
}

} // namespace opensu::library

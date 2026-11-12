#include "linked_play.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace opensu::library {
namespace {

const EmulatorOption* linkCoreOf(const Game& game) {
    const auto found = std::ranges::find_if(game.emulatorOptions, [](const EmulatorOption& option) {
        return option.links && option.installed && option.problem.empty();
    });
    return found == game.emulatorOptions.end() ? nullptr : &*found;
}

std::optional<Game> onLinkCore(const Game& game, std::initializer_list<std::string> arguments) {
    const EmulatorOption* core = linkCoreOf(game);
    if (core == nullptr) {
        return std::nullopt;
    }
    Game linked = game;
    linked.emulator = core->name;
    linked.launch = core->launch;
    linked.launch.args.insert(linked.launch.args.begin(), arguments);
    return linked;
}

std::filesystem::path coreOf(const LaunchSpec& launch) {
    const auto flag = std::ranges::find(launch.args, "--core");
    return flag == launch.args.end() || flag + 1 == launch.args.end()
               ? std::filesystem::path{}
               : std::filesystem::path{*(flag + 1)};
}

} // namespace

bool linkable(const Game& game) {
    return game.source == Source::Rom && game.installed && linkCoreOf(game) != nullptr;
}

std::filesystem::path netplayCore(const Game& game) {
    if (game.source != Source::Rom || !game.installed || linkable(game)) {
        return {};
    }
    return coreOf(game.launch);
}

std::filesystem::path sessionCore(const Game& game) {
    if (const EmulatorOption* core = linkable(game) ? linkCoreOf(game) : nullptr) {
        return coreOf(core->launch);
    }
    return netplayCore(game);
}

std::string sessionCoreId(const Game& game) {
    if (const EmulatorOption* core = linkable(game) ? linkCoreOf(game) : nullptr) {
        return core->id;
    }
    if (netplayCore(game).empty()) {
        return {};
    }
    const auto found =
        std::ranges::find_if(game.emulatorOptions, [&game](const EmulatorOption& option) {
            return option.builtIn && option.installed && option.name == game.emulator;
        });
    return found == game.emulatorOptions.end() ? std::string{} : found->id;
}

bool netplayable(const Game& game) {
    return linkable(game) || !netplayCore(game).empty();
}

std::optional<Game> splitScreenGame(const Game& game) {
    return onLinkCore(game,
                      {"--players", "2", "--option", "mgba_multiplayer_splitscreen=Side by Side"});
}

std::optional<Game> netplayGame(const Game& game, const std::filesystem::path& session) {
    if (!netplayCore(game).empty()) {
        Game played = game;
        played.launch.args.insert(played.launch.args.begin(), {"--netplay", session.string()});
        return played;
    }
    return onLinkCore(game, {"--netplay", session.string(), "--option",
                             "mgba_multiplayer_splitscreen=Side by Side", "--option",
                             "mgba_multiplayer_av=Self"});
}

} // namespace opensu::library

#include "linked_play.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace opensu::library {
namespace {

const EmulatorOption* linkCoreOf(const Game& game) {
    const auto found = std::ranges::find_if(game.emulatorOptions, [](const EmulatorOption& option) {
        return option.id == linkCoreId && option.installed && option.problem.empty();
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

} // namespace

std::string_view linkCoreRevision() noexcept {
    return OPENSU_MGBA_LINK_COMMIT;
}

bool linkable(const Game& game) {
    return game.source == Source::Rom && game.installed && linkCoreOf(game) != nullptr;
}

std::optional<Game> splitScreenGame(const Game& game) {
    return onLinkCore(game,
                      {"--players", "2", "--option", "mgba_multiplayer_splitscreen=Side by Side"});
}

std::optional<Game> netplayGame(const Game& game, const std::filesystem::path& session) {
    return onLinkCore(game, {"--netplay", session.string(), "--option",
                             "mgba_multiplayer_splitscreen=Side by Side", "--option",
                             "mgba_multiplayer_av=Self"});
}

} // namespace opensu::library

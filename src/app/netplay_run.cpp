#include "netplay_run.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "fileio/owner_only_write.hpp"
#include "library/linked_play.hpp"
#include "lucent/log.h"
#include "retro/save_paths.hpp"
#include "secrets/secret.hpp"

namespace opensu::app {
namespace {

/// Player 1's save is the frontend's; the core keeps the others' by player.
std::filesystem::path sessionSaveOf(const std::filesystem::path& folder,
                                    const std::filesystem::path& rom, unsigned slot) {
    const std::string stem = rom.stem().string();
    return folder / (slot == 0 ? stem + ".srm" : stem + ".p" + std::to_string(slot + 1) + ".srm");
}

} // namespace

std::string NetplayRun::contentsOf(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::string NetplayRun::sha256Of(const std::filesystem::path& rom) {
    return secrets::hexText(secrets::crypto::sha256(contentsOf(rom)));
}

const library::Game* NetplayRun::romOf(const std::vector<library::Game>& games,
                                       const lan::wire::NetplayAsk& rom) {
    const auto found = std::ranges::find_if(games, [&rom](const library::Game& game) {
        std::error_code failed;
        return game.source == library::Source::Rom && game.sourceId == rom.console &&
               std::filesystem::file_size(game.file, failed) == rom.size && !failed &&
               sha256Of(game.file) == rom.sha256;
    });
    return found == games.end() ? nullptr : &*found;
}

lan::wire::NetplayFacts NetplayRun::factsOf(const library::Game& game) const {
    const std::filesystem::path bios = hooks_.biosFolder() / library::linkCoreBios;
    std::error_code failed;
    return lan::wire::NetplayFacts{
        .coreRevision = std::string{library::linkCoreRevision()},
        .options = hooks_.linkOptions(game),
        .bios = std::filesystem::is_regular_file(bios, failed) ? sha256Of(bios) : std::string{}};
}

std::filesystem::path NetplayRun::profileSave(const library::Game& game) const {
    return sessionSaveOf(retro::systemSaveFolder(hooks_.profileSaves(), game.sourceId), game.file,
                         0);
}

std::string NetplayRun::begin(const Start& start) {
    std::error_code failed;
    std::filesystem::remove_all(places_.sessionSaves, failed);
    const std::filesystem::path folder =
        retro::systemSaveFolder(places_.sessionSaves, start.game.sourceId);
    std::filesystem::create_directories(folder, failed);
    std::string error;
    for (unsigned slot = 0; slot < start.saves.size(); ++slot) {
        if (!start.saves[slot].empty() &&
            !fileio::writeWhole(sessionSaveOf(folder, start.game.file, slot), start.saves[slot],
                                error)) {
            return error;
        }
    }
    nlohmann::json session{{"slot", start.slot},
                           {"players", start.saves.size()},
                           {"clockMs", start.clock.epochMs},
                           {"utcOffset", start.clock.utcOffset},
                           {"sendKey", secrets::hexText(start.sendKey)},
                           {"receiveKey", secrets::hexText(start.receiveKey)}};
    if (!start.peer.empty()) {
        session["peer"] = start.peer;
    }
    if (start.port != 0) {
        session["port"] = start.port;
    }
    if (start.delay != 0) {
        session["delay"] = start.delay;
    }
    const std::optional<library::Game> game = library::netplayGame(start.game, places_.sessionFile);
    if (!game) {
        return "the link core is gone";
    }
    if (!fileio::writeOwnerOnly(places_.sessionFile, session.dump(), error)) {
        return error;
    }
    lucent::info("netplay", "playing {} as player {}", start.game.title, start.slot + 1);
    if (!hooks_.launch(*game, places_.sessionSaves)) {
        std::filesystem::remove(places_.sessionFile, failed);
        return "its game did not start";
    }
    running_ = Running{.gameId = start.game.id,
                       .own = sessionSaveOf(folder, start.game.file, start.slot),
                       .profile = profileSave(start.game)};
    return {};
}

void NetplayRun::ended(const library::Game& game) {
    if (!running_ || running_->gameId != game.id) {
        return;
    }
    const Running session = *running_;
    running_.reset();
    std::error_code failed;
    std::filesystem::remove(places_.sessionFile, failed);
    if (!std::filesystem::exists(session.own, failed)) {
        lucent::info("netplay", "the session left no save of this player");
        return;
    }
    std::string error;
    if (!fileio::writeWhole(session.profile, contentsOf(session.own), error)) {
        lucent::error("netplay", "the session's save stays at {}: {}", session.own.string(), error);
        hooks_.say("The session's save could not be kept: " + error, true);
        return;
    }
    lucent::info("netplay", "the session's save went back to {}", session.profile.string());
}

} // namespace opensu::app

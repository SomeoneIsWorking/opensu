#include "steam_request.hpp"

#include <chrono>
#include <optional>

#include "command.hpp"
#include "lucent/log.h"

namespace opensu::launch {
namespace {

/// `open` hands the link to Steam, starting it when it is not running, and exits.
constexpr auto openWait = std::chrono::seconds{30};

} // namespace

bool requestSteamRun(const library::Game& game,
                     const std::vector<std::filesystem::path>& /*executablePath*/,
                     std::string& failure) {
    const std::string link = "steam://rungameid/" + game.sourceId;
    const std::optional<int> status = runCommand(
        "/usr/bin/open", {link}, std::chrono::duration_cast<std::chrono::milliseconds>(openWait));
    if (status != 0) {
        lucent::warn("launch", "open {} exited with {}", link, status.value_or(-1));
        failure = "could not ask Steam to start " + game.title;
        return false;
    }
    lucent::info("launch", "asked Steam to run {} through {}", game.title, link);
    return true;
}

} // namespace opensu::launch

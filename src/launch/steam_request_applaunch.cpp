#include "steam_request.hpp"

#include <chrono>
#include <optional>

#include "command.hpp"
#include "lucent/log.h"

namespace opensu::launch {
namespace {

/// `steam -applaunch` only hands the request to the running client.
constexpr auto applaunchWait = std::chrono::seconds{30};

} // namespace

bool requestSteamRun(const library::Game& game,
                     const std::vector<std::filesystem::path>& executablePath,
                     std::string& failure) {
    const std::filesystem::path program = resolveExecutable(game.launch.program, executablePath);
    if (program.empty()) {
        failure = "could not start " + programName(game.launch.program);
        return false;
    }
    const std::optional<int> status =
        runCommand(program.string(), game.launch.args,
                   std::chrono::duration_cast<std::chrono::milliseconds>(applaunchWait));
    if (!status) {
        failure = "could not ask Steam to start " + game.title;
        return false;
    }
    lucent::info("launch", "steam -applaunch for {} exited with {}", game.title, *status);
    return true;
}

} // namespace opensu::launch

#include "handoff.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "command.hpp"
#include "library/game.hpp"
#include "lucent/log.h"
#include "process_tree.hpp"

namespace iideck::launch {
namespace {

using Clock = std::chrono::system_clock;

/// How often the process table is consulted while waiting.
constexpr auto pollInterval = std::chrono::milliseconds{750};

/// `steam -applaunch` only hands the request to the running client.
constexpr auto applaunchWait = std::chrono::seconds{30};

} // namespace

std::string Error::message() const {
    return "cannot launch " + id + ": " + reason;
}

Handoff::Handoff(std::vector<std::filesystem::path> executablePath, std::string session,
                 SteamGate& steam)
    : executablePath_{std::move(executablePath)}, session_{std::move(session)}, steam_{steam} {
}

void Handoff::pause() {
    std::unique_lock lock{wakeMutex_};
    wake_.wait_for(lock, pollInterval, [this] {
        return forced_.load();
    });
}

void Handoff::forceClose() {
    forced_.store(true);
    instance_.kill();
    std::string hint;
    {
        const std::lock_guard lock{hintMutex_};
        hint = steamHint_;
    }
    ProcessTree::killMatching(hint);
    const std::lock_guard lock{wakeMutex_};
    wake_.notify_all();
}

Handoff::Begun Handoff::beginSteam(const library::Game& game, std::string& failure) {
    const std::filesystem::path program = resolveExecutable(game.launch.program, executablePath_);
    if (program.empty()) {
        failure = "could not start " + game.launch.program;
        return Begun::Failed;
    }
    {
        // Set before the request, so a force-close finds a game that appears at once.
        const std::lock_guard lock{hintMutex_};
        steamHint_ = game.processHint;
    }
    const auto forgetHint = [this] {
        const std::lock_guard lock{hintMutex_};
        steamHint_.clear();
    };

    const SteamState state = steam_.waitReady(startTimeout, [this] {
        return forced_.load();
    });
    if (forced_.load()) {
        forgetHint();
        return Begun::Cancelled;
    }
    if (state != SteamState::Ready) {
        forgetHint();
        switch (state) {
        case SteamState::Blocked:
            failure = "Steam is running on the desktop; quit it to use it inside iideck";
            break;
        case SteamState::Initializing:
            failure = "Steam did not become ready";
            break;
        default:
            failure = "Steam failed to start";
            break;
        }
        return Begun::Failed;
    }

    const std::optional<int> status =
        runCommand(program.string(), game.launch.args,
                   std::chrono::duration_cast<std::chrono::milliseconds>(applaunchWait));
    if (!status) {
        forgetHint();
        failure = "could not ask Steam to start " + game.title;
        return Begun::Failed;
    }
    lucent::info("launch", "steam -applaunch for {} exited with {}", game.title, *status);
    return Begun::Started;
}

Handoff::Begun Handoff::beginScope(const library::Game& game,
                                   const std::vector<std::string>& environment,
                                   std::string& failure) {
    if (resolveExecutable(game.launch.program, executablePath_).empty()) {
        failure = "could not start " + game.launch.program;
        return Begun::Failed;
    }
    const std::string unit =
        session_ + "-game-" + std::to_string(launches_.fetch_add(1)) + ".scope";
    if (!instance_.start(unit, game.launch.program, game.launch.args, failure, environment)) {
        return Begun::Failed;
    }
    return Begun::Started;
}

bool Handoff::start(const library::Game& game, const std::function<void()>& hide,
                    const std::function<void()>& show, const std::vector<std::string>& environment,
                    std::string& failure) {
    if (game.launch.empty()) {
        failure = "no launch command for " + game.title;
        return false;
    }
    if (game.processHint.empty()) {
        // Without a hint there is nothing to watch, so the wait could only time out.
        failure = std::string{library::label(game.source)} + " cannot tell when " + game.title +
                  " is running";
        return false;
    }

    const bool viaSteam = game.source == library::Source::Steam;
    forced_.store(false);
    const Begun begun =
        viaSteam ? beginSteam(game, failure) : beginScope(game, environment, failure);
    if (begun == Begun::Failed) {
        return false;
    }
    if (begun == Begun::Cancelled) {
        lucent::info("launch", "{} was cancelled before it started", game.title);
        return true;
    }

    // Hidden once the game is on its way: a game that opens its window at once must
    // not appear over a shell that is still up.
    if (hide) {
        hide();
    }
    lucent::info("launch", "started {}", game.title);

    const auto finish = [&] {
        if (viaSteam) {
            // The client keeps running; only this game's tree is ours to end.
            if (forced_.load()) {
                ProcessTree::killMatching(game.processHint);
            }
            const std::lock_guard lock{hintMutex_};
            steamHint_.clear();
        } else {
            instance_.stop();
        }
        if (show) {
            show();
        }
    };
    // Nothing will appear once the thing that was to start the game is gone.
    const auto alive = [&] {
        return viaSteam ? steam_.state() == SteamState::Ready : instance_.running();
    };

    // Phase one: the game appears. The started process says nothing useful about
    // a successful launch, but the instance emptying out means nothing will appear.
    const Clock::time_point startDeadline = Clock::now() + Handoff::startTimeout;
    while (!forced_.load() && !ProcessTree::anyMatches(game.processHint)) {
        if (!alive()) {
            if (forced_.load()) {
                break;
            }
            failure = game.title + " did not start";
            finish();
            return false;
        }
        if (Clock::now() >= startDeadline) {
            lucent::warn("launch", "{} did not appear within {}", game.title,
                         Handoff::startTimeout);
            failure = game.title + " did not start";
            finish();
            return false;
        }
        pause();
    }
    if (forced_.load()) {
        finish();
        lucent::info("launch", "{} was force-closed", game.title);
        return true;
    }
    lucent::info("launch", "{} is running", game.title);

    // Phase two: the game leaves, or the instance does. No time bound: the player
    // can always force-close from the pad.
    while (!forced_.load() && ProcessTree::anyMatches(game.processHint) &&
           (viaSteam || instance_.running())) {
        pause();
    }

    finish();
    lucent::info("launch", "{} finished", game.title);
    return true;
}

} // namespace iideck::launch

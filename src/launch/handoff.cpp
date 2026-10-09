#include "handoff.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "command.hpp"
#include "library/game.hpp"
#include "lucent/log.h"
#include "process_tree.hpp"

namespace opensu::launch {
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
                 SteamGate& steam, GameWindows* windows)
    : executablePath_{std::move(executablePath)}, session_{std::move(session)}, steam_{steam},
      windows_{windows} {
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

Handoff::Begun Handoff::beginSteam(const library::Game& game, const Hooks& hooks, int& baseline,
                                   std::string& failure) {
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

    if (steam_.state() == SteamState::Initializing && hooks.progress) {
        hooks.progress(LaunchProgress{.stage = LaunchProgress::Stage::WaitingForSteam});
    }
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
            failure = "Steam is running on the desktop; quit it to use it inside opensu";
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

    const SteamAppActivity before = steam_.activity(game.sourceId);
    baseline = before.actionId;
    if (before.running == true) {
        // A second -applaunch only gets Steam's "Game already running" dialog.
        lucent::info("launch", "Steam already runs {}; returning to it", game.title);
        return Begun::Started;
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

Handoff::Shown Handoff::awaitSteamWindow(const library::Game& game, int baseline,
                                         const Report& report, std::string& failure) {
    Clock::time_point deadline = Clock::now() + Handoff::startTimeout;
    bool appeared = false;
    while (!forced_.load()) {
        const SteamAppActivity steam = steam_.activity(game.sourceId);
        const std::vector<pid_t> processes = ProcessTree::treesMatching(game.processHint);
        const bool running = steam.running.value_or(false);
        if (running || !processes.empty()) {
            if (!appeared) {
                lucent::info("launch", "{} is running", game.title);
            }
            appeared = true;
            if (windows_ == nullptr || windows_->anyOwnedBy(processes)) {
                return Shown::Window;
            }
        }
        // Only an action Steam started after the request is this launch's.
        const bool ours = steam.actionId > baseline;
        if (ours && !steam.error.empty() && !running) {
            failure = game.title + ": " + steam.error;
            return Shown::Failed;
        }
        // Steam may run an install script, sync the cloud and wait between processes; the
        // launch is over only when its action is.
        const bool preparing = ours && !steam.actionEnded;
        if (const std::optional<double> update = steam_.updateProgress(game.sourceId)) {
            deadline = Clock::now() + Handoff::startTimeout;
            report(LaunchProgress{.stage = LaunchProgress::Stage::Updating, .fraction = *update});
        } else if (preparing) {
            deadline = Clock::now() + Handoff::startTimeout;
            report(LaunchProgress{.stage = LaunchProgress::Stage::Preparing, .task = steam.task});
        } else if (appeared) {
            // Loading has no bound: a first run can compile shaders for minutes, and B cancels.
            report(LaunchProgress{.stage = LaunchProgress::Stage::Loading});
        } else {
            report(LaunchProgress{.stage = LaunchProgress::Stage::Starting});
        }
        if (appeared && !running && processes.empty() && !preparing) {
            failure = game.title + " closed before it showed a window";
            return Shown::Failed;
        }
        if (steam_.state() != SteamState::Ready) {
            failure = game.title + " did not start";
            return Shown::Failed;
        }
        if (!appeared && Clock::now() >= deadline) {
            lucent::warn("launch", "{} did not appear within {}", game.title,
                         Handoff::startTimeout);
            failure = game.title + " did not start";
            return Shown::Failed;
        }
        pause();
    }
    return Shown::Forced;
}

Handoff::Shown Handoff::awaitScopeWindow(const library::Game& game, const Report& report,
                                         std::string& failure) {
    // The started process says nothing useful about a successful launch, but the instance
    // emptying out means nothing will appear.
    const Clock::time_point deadline = Clock::now() + Handoff::startTimeout;
    bool appeared = false;
    while (!forced_.load()) {
        const std::vector<pid_t> processes = ProcessTree::treesMatching(game.processHint);
        if (!processes.empty()) {
            if (!appeared) {
                lucent::info("launch", "{} is running", game.title);
            }
            appeared = true;
            if (windows_ == nullptr || windows_->anyOwnedBy(processes)) {
                return Shown::Window;
            }
            report(LaunchProgress{.stage = LaunchProgress::Stage::Loading});
            pause();
            continue;
        }
        if (appeared) {
            failure = game.title + " closed before it showed a window";
            return Shown::Failed;
        }
        report(LaunchProgress{.stage = LaunchProgress::Stage::Starting});
        if (!instance_.running()) {
            failure = game.title + " did not start";
            return Shown::Failed;
        }
        if (Clock::now() >= deadline) {
            lucent::warn("launch", "{} did not appear within {}", game.title,
                         Handoff::startTimeout);
            failure = game.title + " did not start";
            return Shown::Failed;
        }
        pause();
    }
    return Shown::Forced;
}

bool Handoff::stillRunning(const library::Game& game, bool viaSteam) {
    if (!viaSteam) {
        return instance_.running() && ProcessTree::anyMatches(game.processHint);
    }
    // Steam's word, or a process of the game's: either alone can lag the other.
    return steam_.activity(game.sourceId).running.value_or(false) ||
           ProcessTree::anyMatches(game.processHint);
}

bool Handoff::start(const library::Game& game, const Hooks& hooks,
                    const std::vector<std::string>& environment, std::string& failure) {
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
    int baseline = 0;
    const Begun begun = viaSteam ? beginSteam(game, hooks, baseline, failure)
                                 : beginScope(game, environment, failure);
    if (begun == Begun::Failed) {
        return false;
    }
    if (begun == Begun::Cancelled) {
        lucent::info("launch", "{} was cancelled before it started", game.title);
        return true;
    }

    lucent::info("launch", "started {}", game.title);

    bool hidden = false;
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
        if (hidden && hooks.show) {
            hooks.show();
        }
    };

    std::optional<LaunchProgress> reported;
    const Report report = [&](const LaunchProgress& now) {
        if (reported != now && hooks.progress) {
            hooks.progress(now);
        }
        reported = now;
    };

    const Shown shown = viaSteam ? awaitSteamWindow(game, baseline, report, failure)
                                 : awaitScopeWindow(game, report, failure);
    if (shown == Shown::Failed) {
        finish();
        return false;
    }
    if (shown == Shown::Forced || forced_.load()) {
        finish();
        lucent::info("launch", "{} was force-closed", game.title);
        return true;
    }
    lucent::info("launch", "{} shows a window", game.title);
    // Hidden only now: until the game shows itself, the shell is what the player sees.
    hidden = true;
    if (hooks.hide) {
        hooks.hide();
    }

    // Phase two: the game leaves. No time bound: the player can always force-close from
    // the pad.
    while (!forced_.load() && stillRunning(game, viaSteam)) {
        pause();
    }

    finish();
    lucent::info("launch", "{} finished", game.title);
    return true;
}

} // namespace opensu::launch

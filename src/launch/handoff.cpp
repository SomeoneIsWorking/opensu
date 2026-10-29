#include "handoff.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "command.hpp"
#include "library/game.hpp"
#include "lucent/log.h"
#include "process_tree.hpp"
#include "steam_request.hpp"

namespace opensu::launch {
namespace {

using Clock = std::chrono::system_clock;

/// How often the process table is consulted while waiting.
constexpr auto pollInterval = std::chrono::milliseconds{750};

} // namespace

std::string Error::message() const {
    return "cannot launch " + id + ": " + reason;
}

Handoff::Handoff(std::vector<std::filesystem::path> executablePath, std::string session,
                 SteamGate& steam, GameWindows* windows, std::filesystem::path outputDir)
    : executablePath_{std::move(executablePath)}, session_{std::move(session)},
      outputDir_{std::move(outputDir)}, steam_{steam}, windows_{windows} {
}

void Handoff::pause() {
    std::unique_lock lock{wakeMutex_};
    wake_.wait_for(lock, pollInterval, [this] {
        return forced_.load();
    });
}

void Handoff::forceClose() {
    forced_.store(true);
    const std::lock_guard lock{wakeMutex_};
    wake_.notify_all();
}

Handoff::Begun Handoff::beginSteam(const library::Game& game, const Hooks& hooks, int& baseline,
                                   std::string& failure) {
    // A launch starts openSU's own Steam if it stopped; Normal's is not openSU's to start.
    if (steam_.ownsClient()) {
        steam_.ensureStarted();
        if (steam_.state() == SteamState::Initializing && hooks.progress) {
            hooks.progress(LaunchProgress{.stage = LaunchProgress::Stage::WaitingForSteam});
        }
        const SteamState state = steam_.waitReady(startTimeout, [this] {
            return forced_.load();
        });
        if (forced_.load()) {
            return Begun::Cancelled;
        }
        if (state != SteamState::Ready) {
            failure = notReadyReason(state);
            return Begun::Failed;
        }
    }

    const SteamAppActivity before = steam_.activity(game.sourceId);
    baseline = before.actionId;
    if (before.running == true) {
        // A second -applaunch only gets Steam's "Game already running" dialog.
        lucent::info("launch", "Steam already runs {}; returning to it", game.title);
        return Begun::Returned;
    }
    if (!requestSteamRun(game, executablePath_, failure)) {
        return Begun::Failed;
    }
    return Begun::Started;
}

Handoff::Begun Handoff::beginScope(const library::Game& game,
                                   const std::vector<std::string>& environment,
                                   std::string& failure) {
    if (!ProcessLauncher::startable(game.launch.program, executablePath_)) {
        failure = "could not start " + programName(game.launch.program);
        return Begun::Failed;
    }
    const std::string name = session_ + "-game-" + std::to_string(launches_.fetch_add(1));
    std::filesystem::path output;
    if (!ProcessLauncher::keepsOutput()) {
        std::error_code error;
        std::filesystem::create_directories(outputDir_, error);
        if (error) {
            failure = "cannot create " + outputDir_.string() + ": " + error.message();
            return Begun::Failed;
        }
        output = outputDir_ / (name + ".log");
        lucent::info("launch", "{} writes its output to {}", game.title, output.string());
    }
    if (!instance_.start(name + ".scope", game.launch.program, game.launch.args, failure,
                         environment, game.launch.workingDirectory, output)) {
        return Begun::Failed;
    }
    return Begun::Started;
}

Handoff::Shown Handoff::awaitSteamWindow(const library::Game& game, int baseline,
                                         const Report& report, std::string& failure) {
    Clock::time_point deadline = Clock::now() + Handoff::startTimeout;
    bool appeared = false;
    // A desktop Steam's game opens its window on the desktop, which Gamescope never shows.
    GameWindows* windows = steam_.showsInSession() ? windows_ : nullptr;
    while (!forced_.load()) {
        const SteamAppActivity steam = steam_.activity(game.sourceId);
        const std::vector<ProcessId> processes = ProcessTree::treesMatching(game.processHint);
        const bool running = steam.running.value_or(false);
        if (running || !processes.empty()) {
            if (!appeared) {
                lucent::info("launch", "{} is running", game.title);
            }
            appeared = true;
            if (windows == nullptr || windows->anyOwnedBy(processes)) {
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
        if (steam_.ownsClient() && steam_.state() != SteamState::Ready) {
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
        const std::vector<ProcessId> processes = ProcessTree::treesMatching(game.processHint);
        // Without a process table the started program is all there is to watch.
        if (!processes.empty() || (!ProcessTree::available() && instance_.startedRunning())) {
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
            if (!instance_.startedRunning()) {
                failure = game.title + " closed before it showed a window";
                return Shown::Failed;
            }
            report(LaunchProgress{.stage = LaunchProgress::Stage::Loading});
            pause();
            continue;
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

void Handoff::endSteamLaunch(const library::Game& game, int baseline, bool requested) {
    const Clock::time_point deadline = Clock::now() + Handoff::startTimeout;
    int cancelled = baseline;
    bool taken = !requested;
    while (true) {
        const SteamAppActivity steam = steam_.activity(game.sourceId);
        const bool ours = steam.actionId > baseline;
        const bool pending = ours && !steam.actionEnded;
        if (pending && steam.actionId > cancelled) {
            steam_.cancelLaunch(game.sourceId);
            cancelled = steam.actionId;
        }
        const bool running = steam.running.value_or(false);
        const bool present = ProcessTree::anyMatches(game.processHint);
        if (present) {
            ProcessTree::killMatching(game.processHint);
        }
        taken = taken || ours || running || present;
        if (taken && !pending && !running && !present) {
            lucent::info("launch", "Steam let go of {}", game.title);
            return;
        }
        if (steam_.ownsClient() && steam_.state() != SteamState::Ready) {
            lucent::warn("launch", "Steam stopped before it let go of {}", game.title);
            return;
        }
        if (Clock::now() >= deadline) {
            lucent::warn("launch", "Steam did not let go of {} within {}", game.title,
                         Handoff::startTimeout);
            return;
        }
        // pause() returns at once now that forced_ is set; Steam's record changes once a second.
        std::this_thread::sleep_for(pollInterval);
    }
}

bool Handoff::stillRunning(const library::Game& game, bool viaSteam) {
    if (!viaSteam) {
        // A command line reads empty while its process execs; the started process's pid does not
        // change.
        return instance_.startedRunning() ||
               (instance_.running() && ProcessTree::anyMatches(game.processHint));
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
    const bool requested = begun == Begun::Started;
    if (begun == Begun::Cancelled) {
        lucent::info("launch", "{} was cancelled before it started", game.title);
        return true;
    }

    lucent::info("launch", "started {}", game.title);

    bool hidden = false;
    const auto finish = [&] {
        if (viaSteam) {
            // The client keeps running; only this launch and this game's tree are ours to end.
            if (forced_.load()) {
                endSteamLaunch(game, baseline, requested);
            }
        } else {
            // SIGTERM first so the game can save; the scope SIGKILLs what is left after 5 s.
            instance_.stop();
            if (!forced_.load() && hooks.ended) {
                hooks.ended(instance_.exitStatus());
            }
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
    // Read after the wait: forceClose() sets forced_ before the game is ended, so one that vanished
    // because the player closed it is not a failure.
    if (shown == Shown::Forced || forced_.load()) {
        failure.clear();
        if (viaSteam) {
            report(LaunchProgress{.stage = LaunchProgress::Stage::Cancelling});
        }
        finish();
        lucent::info("launch", "{} was force-closed", game.title);
        return true;
    }
    if (shown == Shown::Failed) {
        finish();
        return false;
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

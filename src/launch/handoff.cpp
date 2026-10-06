#include "handoff.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include <unistd.h>

#include "library/game.hpp"
#include "lucent/log.h"

namespace iideck::launch {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::system_clock;

/// How often the process table is consulted while waiting.
constexpr auto pollInterval = std::chrono::milliseconds{750};

} // namespace

std::string Error::message() const {
    return "cannot launch " + id + ": " + reason;
}

bool Handoff::processMatches(const std::string& hint) {
    if (hint.empty()) {
        return false;
    }
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{"/proc", ec}) {
        if (ec) {
            return false;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() < '0' || name.front() > '9') {
            continue;
        }
        std::ifstream cmdline{entry.path() / "cmdline", std::ios::binary};
        if (!cmdline) {
            // The process exited between the readdir and the read.
            continue;
        }
        std::string text{std::istreambuf_iterator<char>{cmdline}, std::istreambuf_iterator<char>{}};
        if (text.find(hint) != std::string::npos) {
            return true;
        }
    }
    return false;
}

Handoff::Handoff(std::filesystem::path home, std::vector<std::filesystem::path> executablePath)
    : steam_{std::move(home)}, executablePath_{std::move(executablePath)} {
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
    const std::lock_guard lock{wakeMutex_};
    wake_.notify_all();
}

bool Handoff::start(const library::Game& game, const Output& output, bool insideGamescope,
                    const std::function<void()>& hide, const std::function<void()>& show,
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
    if (!Instance::findExecutable(game.launch.program, executablePath_)) {
        failure = "could not start " + game.launch.program;
        return false;
    }
    if (game.source == library::Source::Steam && steam_.runningOutside(instance_.unit())) {
        failure = "Steam is running on the desktop; quit it to launch inside iideck";
        return false;
    }

    const library::LaunchSpec command =
        insideGamescope ? game.launch : wrapInGamescope(output, game.launch);
    const std::string unit = "iideck-game-" + std::to_string(getpid()) + "-" +
                             std::to_string(launches_.fetch_add(1)) + ".scope";
    forced_.store(false);
    if (!instance_.start(unit, command.program, command.args, failure)) {
        return false;
    }

    // Hidden once the instance exists: a game that opens its window at once must
    // not appear over a shell that is still up.
    if (hide) {
        hide();
    }
    lucent::info("launch", "started {} in {}", game.title, unit);

    const auto finish = [&] {
        instance_.stop();
        if (show) {
            show();
        }
    };

    // Phase one: the game appears. The started process says nothing useful about
    // a successful launch, but the instance emptying out means nothing will appear.
    const Clock::time_point startDeadline = Clock::now() + Handoff::startTimeout;
    while (!forced_.load() && !processMatches(game.processHint)) {
        if (!instance_.running()) {
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
    while (!forced_.load() && processMatches(game.processHint) && instance_.running()) {
        pause();
    }

    finish();
    lucent::info("launch", "{} finished", game.title);
    return true;
}

} // namespace iideck::launch

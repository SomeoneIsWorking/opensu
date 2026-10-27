// A Steam client (`launch::SteamGate`) whose state the test sets: what a launch or a sign-in
// waits on, without a Steam.
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "launch/steam_gate.hpp"

namespace opensu::test {

/// A Steam client whose state the test sets.
class FakeSteamGate final : public launch::SteamGate {
  public:
    explicit FakeSteamGate(launch::SteamState initial = launch::SteamState::Ready)
        : state_{initial} {
    }

    void set(launch::SteamState state) {
        state_.store(state);
    }

    /// An update pending for every app, `progress` of the way through; negative for none.
    void setUpdate(double progress) {
        update_.store(progress);
    }

    std::optional<double> updateProgress(std::string_view) const override {
        const double progress = update_.load();
        if (progress < 0.0) {
            return std::nullopt;
        }
        return progress;
    }

    launch::SteamState state() const override {
        return state_.load();
    }

    /// The test drives the state itself; a launch does not start it.
    void ensureStarted() override {
    }

    bool ownsClient() const override {
        return owns_.load();
    }

    /// Whether openSU is to start and stop this client; false models a desktop Steam (Normal).
    void setOwnsClient(bool owns) {
        owns_.store(owns);
    }

    /// What Steam says about every app from now on.
    void setActivity(launch::SteamAppActivity activity) {
        const std::lock_guard lock{activityMutex_};
        activity_ = std::move(activity);
    }

    launch::SteamAppActivity activity(std::string_view) const override {
        const std::lock_guard lock{activityMutex_};
        return activity_;
    }

    bool showsInSession() const override {
        return inSession_;
    }

    void cancelLaunch(std::string_view) override {
        ++cancels_;
    }

    /// How many times the handoff asked Steam to drop a launch.
    int cancels() const {
        return cancels_.load();
    }

    launch::SteamState waitReady(std::chrono::milliseconds timeout,
                                 const std::function<bool()>& cancelled) override {
        const std::chrono::steady_clock::time_point until =
            std::chrono::steady_clock::now() + timeout;
        while (state_.load() == launch::SteamState::Initializing && !cancelled() &&
               std::chrono::steady_clock::now() < until) {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return state_.load();
    }

    /// A desktop Steam's games show outside openSU's Gamescope.
    void setInSession(bool inSession) {
        inSession_.store(inSession);
    }

  private:
    std::atomic<launch::SteamState> state_;
    std::atomic<double> update_{-1.0};
    mutable std::mutex activityMutex_;
    launch::SteamAppActivity activity_;
    std::atomic<bool> inSession_{true};
    std::atomic<bool> owns_{true};
    std::atomic<int> cancels_{0};
};

} // namespace opensu::test

// launch — what a Steam launch needs from the Steam client iideck owns.
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string_view>

namespace iideck::launch {

/// Where the background Steam client stands.
enum class SteamState {
    /// Not started, or shut down.
    Stopped,
    /// Started, not yet logged on.
    Initializing,
    /// Logged on and able to take a launch.
    Ready,
    /// Could not start, or exited.
    Failed,
    /// A Steam client runs outside iideck, so a launch would escape it.
    Blocked,
};

/// A short lower-case name, as the control channel reports it.
[[nodiscard]] constexpr std::string_view name(SteamState state) noexcept {
    switch (state) {
    case SteamState::Stopped:
        return "stopped";
    case SteamState::Initializing:
        return "initializing";
    case SteamState::Ready:
        return "ready";
    case SteamState::Failed:
        return "failed";
    case SteamState::Blocked:
        return "blocked";
    }
    return "stopped";
}

/// The Steam client as the handoff sees it. Implemented by steam::Client; an
/// interface so the handoff does not depend on the module that starts Steam.
class SteamGate {
  public:
    virtual ~SteamGate() = default;

    /// The current state. Callable from any thread.
    [[nodiscard]] virtual SteamState state() const = 0;

    /// Blocks while the client is initializing, until it is ready, fails, `timeout`
    /// passes or `cancelled` returns true. Returns the state it stopped at.
    [[nodiscard]] virtual SteamState waitReady(std::chrono::milliseconds timeout,
                                               const std::function<bool()>& cancelled) = 0;

    /// How far Steam is through an update it must apply before it runs `appId`, 0 to 1;
    /// nothing when none is pending. Callable from any thread.
    [[nodiscard]] virtual std::optional<double> updateProgress(std::string_view appId) const = 0;
};

} // namespace iideck::launch

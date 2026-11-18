// busy_host — a remote game asked for while its openSU plays another: the question here
// ("<host> is playing <game>"). The profile playing it continues it here or closes it and plays the
// game asked for, as if it sat there; another profile asks to join it. The close goes to that
// openSU off the loop (`LanNode::askToClose`); once it closed and its listing here no longer shows
// it playing, the game asked for starts.
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "lan_service.hpp"
#include "launch_request.hpp"
#include "library/game.hpp"

namespace opensu::app {

class BusyHost {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a failure.
        std::function<void(const std::string& text, bool error)> say;
        /// Starts `game` through the normal launch path.
        std::function<void(const library::Game& game, const LaunchRequest& request)> launch;
    };

    BusyHost(gpu2d::ui::Navigator& navigator, LanService& lan, Hooks hooks)
        : navigator_{navigator}, lan_{lan}, hooks_{std::move(hooks)} {
    }

    /// `wanted` was asked for while its host plays `running`: asks the player what to do.
    void ask(const library::Game& running, const library::Game& wanted,
             const LaunchRequest& request);

    /// Tells how the close stands and starts the game asked for once `games` shows its host
    /// free. Main loop.
    void service(const std::vector<library::Game>& games);

  private:
    using Clock = std::chrono::steady_clock;
    /// What the asking thread leaves for the loop.
    struct Heard {
        std::mutex mutex;
        std::optional<std::string> refusal;
        bool closed{false};
    };
    /// A close under way and the game to start after it.
    struct Pending {
        library::Game running;
        library::Game wanted;
        LaunchRequest request;
        std::shared_ptr<Heard> heard;
        std::optional<Clock::time_point> closedAt;
    };
    /// How long the listing here may take to show the host free once it closed.
    static constexpr std::chrono::seconds listingPatience{15};

    void close(const library::Game& running, const library::Game& wanted,
               const LaunchRequest& request);

    gpu2d::ui::Navigator& navigator_;
    LanService& lan_;
    Hooks hooks_;
    std::optional<Pending> pending_;
};

} // namespace opensu::app

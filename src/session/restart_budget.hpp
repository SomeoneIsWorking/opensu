// session — how often a compositor that ended abnormally is started again.
#pragma once

#include <chrono>
#include <deque>

namespace opensu::session {

/// At most `restarts` restarts within any `window`.
struct RestartLimit {
    int restarts{3};
    std::chrono::seconds window{120};
};

class RestartBudget {
  public:
    using Clock = std::chrono::steady_clock;

    explicit RestartBudget(RestartLimit limit);

    /// Spends one restart at `now` when fewer than the limit were spent in the window before it.
    [[nodiscard]] bool spend(Clock::time_point now);

    [[nodiscard]] const RestartLimit& limit() const noexcept {
        return limit_;
    }

  private:
    RestartLimit limit_;
    std::deque<Clock::time_point> spent_;
};

} // namespace opensu::session

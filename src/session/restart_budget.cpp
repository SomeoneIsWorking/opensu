#include "restart_budget.hpp"

namespace opensu::session {

RestartBudget::RestartBudget(RestartLimit limit) : limit_{limit} {
}

bool RestartBudget::spend(Clock::time_point now) {
    while (!spent_.empty() && now - spent_.front() >= limit_.window) {
        spent_.pop_front();
    }
    if (static_cast<int>(spent_.size()) >= limit_.restarts) {
        return false;
    }
    spent_.push_back(now);
    return true;
}

} // namespace opensu::session

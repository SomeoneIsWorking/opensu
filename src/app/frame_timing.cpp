#include "frame_timing.hpp"

#include <algorithm>

namespace opensu::app {

void FrameTiming::start(Clock::time_point now) {
    if (started_) {
        recent_.push_back(Frame{now, now - last_});
    }
    started_ = true;
    last_ = now;
    while (!recent_.empty() && now - recent_.front().end > window) {
        recent_.pop_front();
    }
}

FrameTiming::Clock::duration FrameTiming::worst() const noexcept {
    Clock::duration longest{};
    for (const Frame& frame : recent_) {
        longest = std::max(longest, frame.length);
    }
    return longest;
}

} // namespace opensu::app

// frame_timing — how long the loop's frames take, start to start, and the longest of the last
// second, which `/state` reports so a hitch is measured in the running product.
#pragma once

#include <chrono>
#include <deque>

namespace opensu::app {

class FrameTiming {
  public:
    using Clock = std::chrono::steady_clock;

    /// How far back `worst` looks.
    static constexpr Clock::duration window = std::chrono::seconds{1};

    /// A frame starts at `now`, ending the one before.
    void start(Clock::time_point now);
    /// The longest frame that ended within `window` of the last start; zero before two starts.
    [[nodiscard]] Clock::duration worst() const noexcept;

  private:
    struct Frame {
        Clock::time_point end;
        Clock::duration length;
    };

    Clock::time_point last_{};
    bool started_{false};
    std::deque<Frame> recent_;
};

} // namespace opensu::app

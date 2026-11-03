// frame_stages — how long each part of the runner's frame takes, logged as percentiles every few
// seconds, so a slow game is measured on the device it runs on.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace opensu::retro {

class FrameStages {
  public:
    using Clock = std::chrono::steady_clock;

    enum class Stage : std::uint8_t {
        /// `retro_run`.
        Core,
        /// Feeding the audio device.
        Audio,
        /// Waiting for the GPU to finish the frame before.
        Fence,
        /// Waiting for a swapchain image.
        Acquire,
        /// Recording and submitting the frame: upload, shader passes, overlay.
        Record,
        /// `vkQueuePresentKHR`.
        Present,
        /// Sleeping to the next frame's time.
        Sleep,
    };
    static constexpr std::size_t stageCount = 7;

    /// How often a line is logged.
    static constexpr Clock::duration window = std::chrono::seconds{5};

    void add(Stage stage, Clock::duration spent);
    /// The frame that started at `start` is done with `audioQueuedMs` of sound queued; logs the
    /// window once it is over.
    void endFrame(Clock::time_point start, double audioQueuedMs);

  private:
    void log(Clock::duration over);

    std::array<std::vector<double>, stageCount> stages_;
    std::vector<double> frames_;
    std::vector<double> audio_;
    Clock::time_point lastStart_{};
    Clock::time_point windowStart_{};
};

} // namespace opensu::retro

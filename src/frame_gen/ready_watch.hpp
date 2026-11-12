// ready_watch — when submissions on a `GeneratorDevice` finish, watched on a thread of its own
// through the timeline semaphore: how long a real frame's first generated frame took to be ready.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#include "frame_gen/generator_gpu.hpp"

namespace opensu::frame_gen {

/// How long a watch waits on the semaphore before it checks it is asked to stop.
inline constexpr std::uint64_t readyWaitSliceNs = 50'000'000;

class ReadyWatch {
  public:
    explicit ReadyWatch(GeneratorDevice& gpu);
    /// Stops the thread within a wait slice and joins it.
    ~ReadyWatch();
    ReadyWatch(const ReadyWatch&) = delete;
    ReadyWatch& operator=(const ReadyWatch&) = delete;
    ReadyWatch(ReadyWatch&&) = delete;
    ReadyWatch& operator=(ReadyWatch&&) = delete;

    /// Times submission `seqNo` from `from`.
    void watch(std::uint64_t seqNo, std::chrono::steady_clock::time_point from);
    /// The times, in nanoseconds, of the submissions finished since the last take, oldest first.
    [[nodiscard]] std::vector<std::uint64_t> take();

  private:
    struct Watched {
        std::uint64_t seqNo;
        std::chrono::steady_clock::time_point from;
    };

    void run(const std::stop_token& stop);

    GeneratorDevice& gpu_;
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Watched> pending_;
    std::vector<std::uint64_t> finished_;
    std::uint64_t timed_{0};
    // Last, so it stops and joins before the rest goes.
    std::jthread thread_;
};

} // namespace opensu::frame_gen

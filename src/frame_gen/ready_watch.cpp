#include "frame_gen/ready_watch.hpp"

#include <stdexcept>
#include <utility>

#include "lucent/log.h"

namespace opensu::frame_gen {

ReadyWatch::ReadyWatch(GeneratorDevice& gpu)
    : gpu_{gpu}, thread_{[this](const std::stop_token& stop) {
          run(stop);
      }} {
}

ReadyWatch::~ReadyWatch() {
    const auto asked = std::chrono::steady_clock::now();
    thread_.request_stop();
    thread_.join();
    const auto took = std::chrono::steady_clock::now() - asked;
    lucent::info("framegen", "ready times stopped after {} frames, joined in {} us", timed_,
                 std::chrono::duration_cast<std::chrono::microseconds>(took).count());
}

void ReadyWatch::watch(std::uint64_t seqNo, std::chrono::steady_clock::time_point from) {
    {
        const std::scoped_lock lock{mutex_};
        pending_.push_back(Watched{.seqNo = seqNo, .from = from});
    }
    wake_.notify_one();
}

std::vector<std::uint64_t> ReadyWatch::take() {
    const std::scoped_lock lock{mutex_};
    return std::exchange(finished_, {});
}

void ReadyWatch::run(const std::stop_token& stop) {
    try {
        while (!stop.stop_requested()) {
            Watched next{};
            {
                std::unique_lock lock{mutex_};
                if (!wake_.wait(lock, stop, [this] {
                        return !pending_.empty();
                    })) {
                    return;
                }
                next = pending_.front();
                pending_.pop_front();
            }
            while (!gpu_.reached(next.seqNo, readyWaitSliceNs)) {
                if (stop.stop_requested()) {
                    return;
                }
            }
            const auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - next.from);
            lucent::debug("framegen", "ready {} us after its real frame came", took.count() / 1000);
            const std::scoped_lock lock{mutex_};
            ++timed_;
            finished_.push_back(static_cast<std::uint64_t>(took.count()));
        }
    } catch (const std::runtime_error& error) {
        // A lost device times nothing more; the pacer keeps the need it has.
        lucent::error("framegen", "ready times stop: {}", error.what());
    }
}

} // namespace opensu::frame_gen

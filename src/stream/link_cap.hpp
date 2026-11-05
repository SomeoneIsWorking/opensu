// link_cap — a test run's stand-in for a slow link: a queue in front of the datagrams a guest
// takes, drained at a rate. A datagram the queue has no room for is dropped as a full link would;
// the rest wait their turn, so a link run over its rate shows its queue as delay before it loses.
// Off (every datagram passed on as it came) unless a rate is set. Safe from any thread.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "token_bucket.hpp"

namespace opensu::stream {

class LinkCap {
  public:
    /// Carries at most `bitsPerSecond` from now on; 0 carries everything.
    void set(std::uint64_t bitsPerSecond);
    /// Queues a datagram arriving at `nowUs`; false when the queue has no room and drops it.
    [[nodiscard]] bool push(std::string_view datagram, std::uint64_t nowUs);
    /// A datagram out of the queue by `nowUs` and when it left; nothing while the next waits.
    struct Out {
        std::string datagram;
        std::uint64_t leftUs{0};
    };
    [[nodiscard]] std::optional<Out> pop(std::uint64_t nowUs);
    /// When the next queued datagram leaves; nothing when the queue is empty.
    [[nodiscard]] std::optional<std::uint64_t> nextUs() const;

  private:
    mutable std::mutex mutex_;
    std::uint64_t bitsPerSecond_{0};
    double depth_{0};
    /// Full while the queue is empty: what it holds is the room left.
    TokenBucket room_;
    std::deque<Out> queued_;
};

} // namespace opensu::stream

// link_cap — a test run's stand-in for a slow link: a token bucket over the datagrams a guest
// takes, which drops what the rate leaves no room for as a full link would. Off (every datagram
// taken) unless a rate is set. Safe from any thread.
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "token_bucket.hpp"

namespace opensu::stream {

class LinkCap {
  public:
    /// Takes at most `bitsPerSecond` from now on; 0 takes everything.
    void set(std::uint64_t bitsPerSecond);
    /// Whether a datagram of `bytes` arriving at `nowUs` gets through.
    [[nodiscard]] bool admit(std::size_t bytes, std::uint64_t nowUs);

  private:
    std::mutex mutex_;
    std::uint64_t bitsPerSecond_{0};
    TokenBucket bucket_;
};

} // namespace opensu::stream

// token_bucket — bytes allowed at a rate: it fills at `bytesPerSecond` up to `depth` bytes, and a
// datagram leaves (or is let in) when the bucket holds its bytes. Pure, with the time passed in;
// `LinkCap` drops what it has no room for, the host's pacer holds it back until there is.
#pragma once

#include <cstddef>
#include <cstdint>

namespace opensu::stream {

class TokenBucket {
  public:
    /// Fills at `bytesPerSecond` up to `depth` bytes from now on, full at the next call.
    void set(double bytesPerSecond, double depth);
    /// Whether `bytes` fit at `nowUs`; they are taken when they do.
    [[nodiscard]] bool take(std::size_t bytes, std::uint64_t nowUs);
    /// How long from `nowUs` until `bytes` fit, 0 when they do now.
    [[nodiscard]] std::uint64_t waitUs(std::size_t bytes, std::uint64_t nowUs);

  private:
    void fill(std::uint64_t nowUs);

    double bytesPerSecond_{0};
    double depth_{0};
    double tokens_{0};
    bool started_{false};
    std::uint64_t filledUs_{0};
};

} // namespace opensu::stream

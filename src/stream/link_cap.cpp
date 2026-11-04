#include "link_cap.hpp"

namespace opensu::stream {
namespace {

/// What the bucket holds when full: 20 ms of the rate, as a link's queue would.
constexpr double depthSeconds = 0.02;

} // namespace

void LinkCap::set(std::uint64_t bitsPerSecond) {
    const std::lock_guard lock{mutex_};
    bitsPerSecond_ = bitsPerSecond;
    const double bytesPerSecond = static_cast<double>(bitsPerSecond) / 8;
    bucket_.set(bytesPerSecond, bytesPerSecond * depthSeconds);
}

bool LinkCap::admit(std::size_t bytes, std::uint64_t nowUs) {
    const std::lock_guard lock{mutex_};
    return bitsPerSecond_ == 0 || bucket_.take(bytes, nowUs);
}

} // namespace opensu::stream

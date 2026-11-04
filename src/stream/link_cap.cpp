#include "link_cap.hpp"

#include <algorithm>

namespace opensu::stream {
namespace {

/// What the bucket holds when full: 20 ms of the rate, as a link's queue would.
constexpr double depthSeconds = 0.02;
constexpr double microsPerSecond = 1e6;

} // namespace

void LinkCap::set(std::uint64_t bitsPerSecond) {
    const std::lock_guard lock{mutex_};
    bitsPerSecond_ = bitsPerSecond;
    tokens_ = 0;
    filledUs_ = 0;
}

bool LinkCap::admit(std::size_t bytes, std::uint64_t nowUs) {
    const std::lock_guard lock{mutex_};
    if (bitsPerSecond_ == 0) {
        return true;
    }
    const double bytesPerSecond = static_cast<double>(bitsPerSecond_) / 8;
    const double depth = bytesPerSecond * depthSeconds;
    if (filledUs_ == 0) {
        tokens_ = depth;
    } else {
        tokens_ =
            std::min(depth, tokens_ + bytesPerSecond * static_cast<double>(nowUs - filledUs_) /
                                          microsPerSecond);
    }
    filledUs_ = nowUs;
    if (tokens_ < static_cast<double>(bytes)) {
        return false;
    }
    tokens_ -= static_cast<double>(bytes);
    return true;
}

} // namespace opensu::stream

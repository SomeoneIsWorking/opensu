#include "token_bucket.hpp"

#include <algorithm>
#include <cmath>

namespace opensu::stream {
namespace {

constexpr double microsPerSecond = 1e6;

} // namespace

void TokenBucket::set(double bytesPerSecond, double depth) {
    bytesPerSecond_ = bytesPerSecond;
    depth_ = depth;
    started_ = false;
}

void TokenBucket::changeRate(double bytesPerSecond, std::uint64_t nowUs) {
    fill(nowUs);
    bytesPerSecond_ = bytesPerSecond;
}

void TokenBucket::fill(std::uint64_t nowUs) {
    if (!started_) {
        tokens_ = depth_;
        started_ = true;
    } else if (nowUs > filledUs_) {
        tokens_ =
            std::min(depth_, tokens_ + bytesPerSecond_ * static_cast<double>(nowUs - filledUs_) /
                                           microsPerSecond);
    }
    filledUs_ = std::max(filledUs_, nowUs);
}

bool TokenBucket::take(std::size_t bytes, std::uint64_t nowUs) {
    fill(nowUs);
    if (tokens_ < static_cast<double>(bytes)) {
        return false;
    }
    tokens_ -= static_cast<double>(bytes);
    return true;
}

std::uint64_t TokenBucket::waitUs(std::size_t bytes, std::uint64_t nowUs) {
    fill(nowUs);
    const double missing = static_cast<double>(bytes) - tokens_;
    if (missing <= 0 || bytesPerSecond_ <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(std::ceil(missing * microsPerSecond / bytesPerSecond_));
}

} // namespace opensu::stream

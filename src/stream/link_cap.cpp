#include "link_cap.hpp"

#include <utility>

namespace opensu::stream {
namespace {

/// The queue: 20 ms of the rate, as a link's would hold.
constexpr double depthSeconds = 0.02;

} // namespace

void LinkCap::set(std::uint64_t bitsPerSecond) {
    const std::lock_guard lock{mutex_};
    bitsPerSecond_ = bitsPerSecond;
    const double bytesPerSecond = static_cast<double>(bitsPerSecond) / 8;
    depth_ = bytesPerSecond * depthSeconds;
    room_.set(bytesPerSecond, depth_);
}

bool LinkCap::push(std::string_view datagram, std::uint64_t nowUs) {
    const std::lock_guard lock{mutex_};
    if (bitsPerSecond_ == 0) {
        queued_.push_back(Out{.datagram = std::string{datagram}, .leftUs = nowUs});
        return true;
    }
    if (!room_.take(datagram.size(), nowUs)) {
        return false;
    }
    // It leaves when everything queued before it and itself have drained: when the room is back.
    const std::uint64_t leftUs = nowUs + room_.waitUs(static_cast<std::size_t>(depth_), nowUs);
    queued_.push_back(Out{.datagram = std::string{datagram}, .leftUs = leftUs});
    return true;
}

std::optional<LinkCap::Out> LinkCap::pop(std::uint64_t nowUs) {
    const std::lock_guard lock{mutex_};
    if (queued_.empty() || queued_.front().leftUs > nowUs) {
        return std::nullopt;
    }
    Out out = std::move(queued_.front());
    queued_.pop_front();
    return out;
}

std::optional<std::uint64_t> LinkCap::nextUs() const {
    const std::lock_guard lock{mutex_};
    if (queued_.empty()) {
        return std::nullopt;
    }
    return queued_.front().leftUs;
}

} // namespace opensu::stream

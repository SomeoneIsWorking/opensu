#include "frame_gen/ready_need.hpp"

#include <algorithm>

namespace opensu::frame_gen {

void ReadyNeed::add(std::uint64_t sampleNs, std::uint64_t refreshNs) {
    samples_[next_] = sampleNs;
    next_ = (next_ + 1) % samples_.size();
    count_ = std::min(count_ + 1, samples_.size());
    if (count_ < samples_.size()) {
        return;
    }
    if (!need_) {
        need_ = refreshNs;
        sinceChange_ = 0;
    }
    if (sampleNs + readyHeadroomNs > *need_) {
        need_ = sampleNs + readyHeadroomNs;
        sinceChange_ = 0;
        return;
    }
    if (++sinceChange_ < samples_.size()) {
        return;
    }
    const std::uint64_t wanted = std::max(slowest() + readyHeadroomNs, readyFloorNs);
    if (wanted < *need_) {
        need_ = std::max(wanted, *need_ > readyLowerStepNs ? *need_ - readyLowerStepNs : 0);
    }
    sinceChange_ = 0;
}

std::uint64_t ReadyNeed::needNs(std::uint64_t refreshNs) const noexcept {
    return need_.value_or(refreshNs);
}

void ReadyNeed::reset() noexcept {
    count_ = 0;
    next_ = 0;
    need_.reset();
    sinceChange_ = 0;
}

std::uint64_t ReadyNeed::slowest() const noexcept {
    return *std::max_element(samples_.begin(),
                             samples_.begin() + static_cast<std::ptrdiff_t>(count_));
}

} // namespace opensu::frame_gen

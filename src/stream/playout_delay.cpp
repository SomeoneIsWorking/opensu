#include "playout_delay.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace opensu::stream {

void PlayoutDelay::decoded(std::uint64_t sentUs, std::uint64_t decodedUs) {
    samples_.push_back(
        Sample{.decodedUs = decodedUs, .lateUs = decodedUs - std::min(sentUs, decodedUs)});
}

std::uint64_t PlayoutDelay::refresh(std::uint64_t nowUs) {
    while (!samples_.empty() && samples_.front().decodedUs + playoutWindowUs < nowUs) {
        samples_.pop_front();
    }
    std::uint64_t wanted = 0;
    if (!samples_.empty()) {
        std::vector<std::uint64_t> late;
        late.reserve(samples_.size());
        for (const Sample& sample : samples_) {
            late.push_back(sample.lateUs);
        }
        const auto at =
            static_cast<std::size_t>(static_cast<double>(late.size() - 1) * playoutShare);
        std::ranges::nth_element(late, late.begin() + static_cast<std::ptrdiff_t>(at));
        wanted = std::min(late[at], playoutMostUs);
    }
    delayUs_ = wanted >= delayUs_
                   ? wanted
                   : std::max(wanted, delayUs_ - std::min(delayUs_, playoutShrinkUs));
    return delayUs_;
}

} // namespace opensu::stream

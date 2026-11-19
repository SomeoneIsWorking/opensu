#include "pyrowave_fit.hpp"

namespace opensu::stream {

bool PyroWaveFit::take(Counts totals, std::uint64_t nowUs) {
    if (givenUp_) {
        return false;
    }
    if (!start_ || nowUs < start_->atUs) {
        start_ = Mark{.atUs = nowUs, .counts = totals};
        return false;
    }
    if (nowUs - start_->atUs < pyroWaveFitWindowUs) {
        return false;
    }
    const Counts& from = start_->counts;
    const std::size_t decoded = totals.decoded - from.decoded;
    const std::size_t missed = (totals.dropped - from.dropped) + (totals.late - from.late);
    const std::size_t frames = decoded + missed;
    start_ = Mark{.atUs = nowUs, .counts = totals};
    if (frames < pyroWaveFitLeast) {
        return false;
    }
    const double share = static_cast<double>(missed) / static_cast<double>(frames);
    if (share <= pyroWaveFitLimit) {
        return false;
    }
    missed_ = share;
    givenUp_ = true;
    return true;
}

} // namespace opensu::stream

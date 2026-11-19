#include "guest_fit.hpp"

#include <algorithm>

namespace opensu::stream {

namespace {

/// A step must be this far under the rate that arrived to lower it.
constexpr double arrivedMargin = 0.95;

double shareOf(std::size_t part, std::size_t whole) {
    return whole == 0 ? 0.0 : static_cast<double>(part) / static_cast<double>(whole);
}

} // namespace

bool GuestFit::take(Counts totals, bool pyroWave, std::uint64_t nowUs) {
    if (!start_ || nowUs < start_->atUs || start_->pyroWave != pyroWave) {
        start_ = Mark{.atUs = nowUs, .counts = totals, .pyroWave = pyroWave};
        return false;
    }
    if (nowUs - start_->atUs < guestFitWindowUs) {
        return false;
    }
    const std::uint64_t elapsedUs = nowUs - start_->atUs;
    const Counts& from = start_->counts;
    const std::size_t decoded = totals.decoded - from.decoded;
    const std::size_t lost = (totals.dropped - from.dropped) + (totals.late - from.late);
    const std::size_t shown = totals.presented - from.presented;
    const std::size_t behind = decoded - std::min(decoded, shown);
    start_ = Mark{.atUs = nowUs, .counts = totals, .pyroWave = pyroWave};
    if (decoded + lost < guestFitLeast) {
        return false;
    }
    if (pyroWave && !pyroWaveGivenUp_) {
        if (const double share = shareOf(lost, decoded + lost); share > pyroWaveFitLimit) {
            missed_ = share;
            pyroWaveGivenUp_ = true;
            return true;
        }
    }
    missed_ = shareOf(behind, decoded);
    if (missed_ > behindLimit) {
        calm_ = 0;
        const double arrived = static_cast<double>(decoded) * 1e6 / static_cast<double>(elapsedUs);
        std::size_t lower = step_ + 1;
        while (lower < frameRateSteps.size() &&
               frameRateSteps.at(lower) >= arrived * arrivedMargin) {
            ++lower;
        }
        if (lower >= frameRateSteps.size()) {
            return false;
        }
        if (raised_) {
            calmNeeded_ = std::min(calmNeeded_ * 2, mostCleanWindows);
        }
        raised_ = false;
        step_ = lower;
        return true;
    }
    if (missed_ >= behindCalm || step_ == 0) {
        calm_ = 0;
        return false;
    }
    if (++calm_ < calmNeeded_) {
        return false;
    }
    calm_ = 0;
    raised_ = true;
    --step_;
    return true;
}

} // namespace opensu::stream

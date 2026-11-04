// percentile — where a share of measured values lies, for timing figures (the runner's frame
// stages).
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace opensu::clock {

/// The value `share` (0 to 1) of the way through `sorted`, which is not empty and ascends.
[[nodiscard]] inline double percentile(const std::vector<double>& sorted, double share) noexcept {
    return sorted[std::min(sorted.size() - 1,
                           static_cast<std::size_t>(share * static_cast<double>(sorted.size())))];
}

} // namespace opensu::clock

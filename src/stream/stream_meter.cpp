#include "stream_meter.hpp"

#include <cmath>

namespace opensu::stream {
namespace {

constexpr double microsPerSecond = 1'000'000.0;
constexpr double bitsPerTenthOfMegabit = 100'000.0;

int rounded(double value) noexcept {
    return static_cast<int>(std::lround(value));
}

} // namespace

StreamQuality qualityOf(const StreamReading& reading) noexcept {
    const int seen = reading.framesPerSecond + reading.lostPerSecond;
    const double lost =
        seen > 0 ? static_cast<double>(reading.lostPerSecond) / static_cast<double>(seen) : 0.0;
    if (reading.latencyMs > poorLatencyMs || reading.roundTripMs > poorRoundTripMs ||
        lost > poorLost) {
        return StreamQuality::Poor;
    }
    if (reading.latencyMs > fairLatencyMs || reading.roundTripMs > fairRoundTripMs ||
        lost > fairLost) {
        return StreamQuality::Fair;
    }
    return StreamQuality::Good;
}

std::optional<StreamReading> StreamMeter::take(const StreamCounts& counts, std::uint64_t nowUs) {
    if (!sinceUs_) {
        sinceUs_ = nowUs;
        since_ = counts;
        return std::nullopt;
    }
    if (nowUs < *sinceUs_ + meterPeriodUs) {
        return std::nullopt;
    }
    const double seconds = static_cast<double>(nowUs - *sinceUs_) / microsPerSecond;
    const auto perSecond = [seconds](std::size_t now, std::size_t before) {
        return now > before ? rounded(static_cast<double>(now - before) / seconds) : 0;
    };
    StreamReading reading{.latencyMs = rounded(counts.latencyMs),
                          .roundTripMs = rounded(static_cast<double>(counts.roundTripUs) / 1000.0),
                          .tenthsOfMegabits = rounded(static_cast<double>(counts.bytesPerSecond) *
                                                      8.0 / bitsPerTenthOfMegabit),
                          .framesPerSecond = perSecond(counts.shown, since_.shown),
                          .lostPerSecond = perSecond(counts.lost, since_.lost)};
    reading.quality = qualityOf(reading);
    sinceUs_ = nowUs;
    since_ = counts;
    return reading;
}

} // namespace opensu::stream

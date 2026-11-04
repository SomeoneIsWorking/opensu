#include "sd_bus.hpp"

#include <cstdint>
#include <cstring>

#include <time.h>

namespace opensu::host::sdbus {
namespace {

/// The time on sd-bus's clock, in microseconds.
std::uint64_t monotonicMicroseconds() {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    constexpr std::uint64_t microsecondsPerSecond = 1000000;
    constexpr std::uint64_t nanosecondsPerMicrosecond = 1000;
    return static_cast<std::uint64_t>(now.tv_sec) * microsecondsPerSecond +
           static_cast<std::uint64_t>(now.tv_nsec) / nanosecondsPerMicrosecond;
}

} // namespace

std::string BusError::text(int code) const {
    return error_.message != nullptr ? error_.message : std::strerror(-code);
}

int pollTimeout(sd_bus* bus) {
    std::uint64_t until = 0;
    if (sd_bus_get_timeout(bus, &until) < 0 || until == UINT64_MAX) {
        return -1;
    }
    const std::uint64_t now = monotonicMicroseconds();
    constexpr std::uint64_t microsecondsPerMillisecond = 1000;
    return until <= now ? 0
                        : static_cast<int>((until - now + microsecondsPerMillisecond - 1) /
                                           microsecondsPerMillisecond);
}

} // namespace opensu::host::sdbus

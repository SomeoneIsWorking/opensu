#include "local_time.hpp"

#include <chrono>

namespace opensu::clock {

std::int32_t secondsEastOfUtc(std::time_t moment) {
    using namespace std::chrono;
    const std::tm local = localTime(moment);
    const sys_days date = year{local.tm_year + 1900} /
                          month{static_cast<unsigned>(local.tm_mon + 1)} /
                          day{static_cast<unsigned>(local.tm_mday)};
    const sys_seconds shown =
        date + hours{local.tm_hour} + minutes{local.tm_min} + seconds{local.tm_sec};
    return static_cast<std::int32_t>((shown - sys_seconds{seconds{moment}}).count());
}

bool timeFormatIs24Hour(std::string_view format) noexcept {
    for (std::size_t at = format.find('%'); at != std::string_view::npos && at + 1 < format.size();
         at = format.find('%', at + 2)) {
        const char directive = format[at + 1];
        if (directive == 'I' || directive == 'l' || directive == 'r') {
            return false;
        }
    }
    return true;
}

} // namespace opensu::clock

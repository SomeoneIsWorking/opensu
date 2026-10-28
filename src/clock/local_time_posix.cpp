#include "local_time.hpp"

#include <langinfo.h>
#include <locale.h>

#include "lucent/log.h"

namespace opensu::clock {

std::tm localTime(std::time_t moment) {
    std::tm parts{};
    localtime_r(&moment, &parts);
    return parts;
}

bool localeClock24Hour() {
    const locale_t locale = newlocale(LC_TIME_MASK, "", static_cast<locale_t>(nullptr));
    if (locale == static_cast<locale_t>(nullptr)) {
        lucent::warn("config", "the time locale could not be loaded; using a 24-hour clock");
        return true;
    }
    const bool twentyFour = timeFormatIs24Hour(nl_langinfo_l(T_FMT, locale));
    freelocale(locale);
    return twentyFour;
}

} // namespace opensu::clock

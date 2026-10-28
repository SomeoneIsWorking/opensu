#include "local_time.hpp"

#include <array>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "lucent/log.h"

namespace opensu::clock {

std::tm localTime(std::time_t moment) {
    std::tm parts{};
    localtime_s(&parts, &moment);
    return parts;
}

bool localeClock24Hour() {
    // The user's long time format: "HH:mm:ss" is 24-hour, "h:mm:ss tt" is 12-hour.
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> format{};
    const int length = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, format.data(),
                                       static_cast<int>(format.size()));
    if (length <= 0) {
        lucent::warn("config", "the time format could not be read; using a 24-hour clock");
        return true;
    }
    return std::wstring_view{format.data()}.find(L'H') != std::wstring_view::npos;
}

} // namespace opensu::clock

#include "clock_text.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace opensu::ui {

std::string ClockText::format(int hour, int minute, bool twentyFourHour) {
    char text[16]{};
    if (twentyFourHour) {
        std::snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
        return text;
    }
    // "h:mm a": 12-hour without a leading zero, then the day half.
    const int twelve = hour % 12 == 0 ? 12 : hour % 12;
    std::snprintf(text, sizeof(text), "%d:%02d %s", twelve, minute, hour < 12 ? "AM" : "PM");
    return text;
}

std::chrono::milliseconds ClockText::untilNextMinute(int second, int millisecond) noexcept {
    const long long wait = 60000LL - (static_cast<long long>(second) * 1000LL + millisecond);
    return std::chrono::milliseconds{std::clamp(wait, 1000LL, 60000LL)};
}

bool ClockText::hasLetters(std::string_view text) noexcept {
    return std::ranges::any_of(text, [](char c) {
        return std::isalpha(static_cast<unsigned char>(c)) != 0;
    });
}

} // namespace opensu::ui

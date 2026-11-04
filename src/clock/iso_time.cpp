#include "iso_time.hpp"

#include <charconv>
#include <chrono>
#include <system_error>

namespace opensu::clock {
namespace {

/// The digits at `at` to `at + count` as a number, or -1.
int digits(std::string_view text, std::size_t at, std::size_t count) {
    if (at + count > text.size()) {
        return -1;
    }
    int value = 0;
    const char* first = text.data() + at;
    const auto [end, error] = std::from_chars(first, first + count, value);
    return error == std::errc{} && end == first + count ? value : -1;
}

} // namespace

std::optional<std::int64_t> parseIsoTime(std::string_view text) {
    // YYYY-MM-DDTHH:MM:SS
    constexpr std::size_t clockEnd = 19;
    if (text.size() < clockEnd || text[4] != '-' || text[7] != '-' ||
        (text[10] != 'T' && text[10] != ' ') || text[13] != ':' || text[16] != ':') {
        return std::nullopt;
    }
    const int year = digits(text, 0, 4);
    const int month = digits(text, 5, 2);
    const int day = digits(text, 8, 2);
    const int hour = digits(text, 11, 2);
    const int minute = digits(text, 14, 2);
    const int second = digits(text, 17, 2);
    if (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0 || second < 0) {
        return std::nullopt;
    }
    const std::chrono::year_month_day date{std::chrono::year{year},
                                           std::chrono::month{static_cast<unsigned>(month)},
                                           std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok() || hour > 23 || minute > 59 || second > 60) {
        return std::nullopt;
    }
    std::size_t at = clockEnd;
    if (at < text.size() && text[at] == '.') {
        ++at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            ++at;
        }
    }
    std::chrono::seconds offset{0};
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
        const int hours = digits(text, at + 1, 2);
        const bool colon = at + 3 < text.size() && text[at + 3] == ':';
        const int minutes = digits(text, at + (colon ? 4 : 3), 2);
        if (hours < 0 || minutes < 0) {
            return std::nullopt;
        }
        offset = (text[at] == '+' ? 1 : -1) *
                 (std::chrono::hours{hours} + std::chrono::minutes{minutes});
        at += colon ? 6 : 5;
    } else if (at < text.size() && text[at] == 'Z') {
        ++at;
    }
    if (at != text.size()) {
        return std::nullopt;
    }
    const std::chrono::sys_seconds moment = std::chrono::sys_days{date} + std::chrono::hours{hour} +
                                            std::chrono::minutes{minute} +
                                            std::chrono::seconds{second} - offset;
    return moment.time_since_epoch().count();
}

} // namespace opensu::clock

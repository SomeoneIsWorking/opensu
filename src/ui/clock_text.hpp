// clock_text — the top bar's clock string and when it next changes (iiSU o28.g, k42).
#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace opensu::ui {

class ClockText {
  public:
    /// iiSU o28.g: "HH:mm" when the system uses 24-hour time, else "h:mm a".
    [[nodiscard]] static std::string format(int hour, int minute, bool twentyFourHour);

    /// iiSU k42: the wait until the next minute boundary, clamped to 1 to 60 seconds.
    [[nodiscard]] static std::chrono::milliseconds untilNextMinute(int second,
                                                                   int millisecond) noexcept;

    /// Whether a clock string has letters, which widens iiSU's status pill (a32.o).
    [[nodiscard]] static bool hasLetters(std::string_view text) noexcept;
};

} // namespace opensu::ui

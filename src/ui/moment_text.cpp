#include "moment_text.hpp"

#include <ctime>

#include "clock/local_time.hpp"

namespace opensu::ui {

std::string momentText(std::chrono::system_clock::time_point at) {
    const std::tm parts = clock::localTime(std::chrono::system_clock::to_time_t(at));
    std::string text(32, '\0');
    text.resize(std::strftime(text.data(), text.size(), "%Y-%m-%d %H:%M", &parts));
    return text;
}

} // namespace opensu::ui

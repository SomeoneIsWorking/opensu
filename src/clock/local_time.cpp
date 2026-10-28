#include "local_time.hpp"

namespace opensu::clock {

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

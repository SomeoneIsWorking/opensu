#include "file_time.hpp"

namespace opensu::clock {

std::chrono::nanoseconds sinceEpoch(std::filesystem::file_time_type written) {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    const auto system = std::chrono::clock_cast<std::chrono::system_clock>(written);
#else
    const auto system = std::chrono::file_clock::to_sys(written);
#endif
    return std::chrono::duration_cast<std::chrono::nanoseconds>(system.time_since_epoch());
}

std::filesystem::file_time_type fileTimeOf(std::chrono::system_clock::time_point at) {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    const auto file = std::chrono::clock_cast<std::chrono::file_clock>(at);
#else
    const auto file = std::chrono::file_clock::from_sys(at);
#endif
    return std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(file);
}

} // namespace opensu::clock

#include "dns_sd.hpp"

namespace opensu::lan {
namespace {

#if defined(_WIN32)
constexpr std::string_view missing = "Finding other openSUs is not built for Windows yet";
#elif defined(__APPLE__)
constexpr std::string_view missing = "Finding other openSUs is not built for macOS yet";
#else
constexpr std::string_view missing = "Finding other openSUs is not built for this system yet";
#endif

} // namespace

std::string machineName() {
    return "openSU";
}

std::string_view discoveryMissing() noexcept {
    return missing;
}

std::unique_ptr<ServiceDiscovery> startDiscovery(const Announcement& /*self*/, std::string& error) {
    error = std::string{missing};
    return nullptr;
}

} // namespace opensu::lan

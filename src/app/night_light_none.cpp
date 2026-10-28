#include "night_light.hpp"

namespace opensu::app {

std::unique_ptr<DesktopNight> openDesktopNight(const std::filesystem::path& /*xdgConfigHome*/) {
    return nullptr;
}

std::unique_ptr<NightLightOutput> openNightLightOutput(bool /*loginSession*/,
                                                       bool /*insideGamescope*/) {
    return nullptr;
}

} // namespace opensu::app

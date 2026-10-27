#include "night_light.hpp"

namespace opensu::app {

std::unique_ptr<NightLightOutput> openNightLightOutput(bool /*loginSession*/,
                                                       bool /*insideGamescope*/) {
    return nullptr;
}

} // namespace opensu::app

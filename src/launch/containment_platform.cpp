#include "containment.hpp"

#include "lucent/log.h"

namespace opensu::launch {

Containment chooseContainment() {
    lucent::info("launch", "launches run in the platform's own process groups");
    return Containment::platform;
}

} // namespace opensu::launch

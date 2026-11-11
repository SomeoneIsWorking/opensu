#include "containment.hpp"

#include <memory>
#include <stdexcept>
#include <string>

#include "launch_group.hpp"
#include "lucent/log.h"

namespace opensu::launch {

Containment chooseContainment() {
    std::string why;
    if (systemdScopeWorks(why)) {
        lucent::info("launch", "launches run in systemd user scopes");
        return Containment::systemdScope;
    }
    lucent::error("launch", "no launch can start: {}", why);
    return Containment::systemdScope;
}

std::unique_ptr<LaunchGroup> makeLaunchGroup(Containment containment) {
    switch (containment) {
    case Containment::systemdScope:
        return makeSystemdScope();
    case Containment::platform:
        break;
    }
    throw std::invalid_argument{"Linux launches have no platform group"};
}

} // namespace opensu::launch

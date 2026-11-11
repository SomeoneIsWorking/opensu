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
    lucent::warn("launch",
                 "launches run under openSU's own keeper, as systemd user scopes cannot: {}", why);
    return Containment::keeper;
}

std::unique_ptr<LaunchGroup> makeLaunchGroup(Containment containment) {
    switch (containment) {
    case Containment::systemdScope:
        return makeSystemdScope();
    case Containment::keeper:
        return makeKeeper();
    case Containment::platform:
        break;
    }
    throw std::invalid_argument{"Linux launches have no platform group"};
}

} // namespace opensu::launch

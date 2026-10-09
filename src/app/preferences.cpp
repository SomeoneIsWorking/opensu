#include "preferences.hpp"

#include <string>

#include "lucent/log.h"

namespace opensu::app {

void Preferences::save() {
    std::string error;
    if (!store_.save(values_, error)) {
        lucent::error("settings", "{}", error);
        onFailure_("cannot keep your settings: " + error);
    }
}

} // namespace opensu::app

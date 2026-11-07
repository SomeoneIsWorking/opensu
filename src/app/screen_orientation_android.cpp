#include "screen_orientation.hpp"

#include "activity_call.hpp"
#include "lucent/log.h"

namespace opensu::app::screen_orientation {

bool turnable() noexcept {
    return true;
}

void stand(bool portrait) {
    const android::ActivityCall call;
    if (!call.ready()) {
        lucent::warn("shell", "the screen cannot be turned: no Activity");
        return;
    }
    const jmethodID rest = call.method("standShell", "(Z)V");
    if (rest == nullptr) {
        lucent::warn("shell", "the Activity has no standShell: {}", call.takeException());
        return;
    }
    call.env()->CallVoidMethod(call.activity(), rest, static_cast<jboolean>(portrait));
    if (const std::string failure = call.takeException(); !failure.empty()) {
        lucent::warn("shell", "the screen cannot be turned: {}", failure);
        return;
    }
    lucent::info("shell", "standing in {}", portrait ? "portrait" : "landscape");
}

} // namespace opensu::app::screen_orientation

#include "shell_handoff.hpp"

#include "activity_call.hpp"
#include "lucent/log.h"

namespace opensu::retro::shell_handoff {

bool active() noexcept {
    return true;
}

bool openMenu() {
    const android::ActivityCall call;
    if (!call.ready()) {
        lucent::warn("retro", "the shell's menu cannot be asked for: no Activity");
        return false;
    }
    call.env()->CallVoidMethod(call.activity(), call.method("openShellMenu", "()V"));
    if (const std::string failure = call.takeException(); !failure.empty()) {
        lucent::warn("retro", "the shell's menu cannot be asked for: {}", failure);
        return false;
    }
    lucent::info("retro", "the shell's menu asked for");
    return true;
}

} // namespace opensu::retro::shell_handoff

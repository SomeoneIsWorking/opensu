#include "activity_call.hpp"
#include "link_hold.hpp"
#include "lucent/log.h"

namespace opensu::stream {

void holdLink(bool held) {
    const android::ActivityCall call;
    if (!call.ready()) {
        return;
    }
    call.env()->CallVoidMethod(call.activity(), call.method("holdWifi", "(Z)V"),
                               static_cast<jboolean>(held));
    if (const std::string thrown = call.takeException(); !thrown.empty()) {
        lucent::warn("stream", "the Wi-Fi lock failed: {}", thrown);
    }
}

} // namespace opensu::stream

#include "activity_call.hpp"
#include "android_apps.hpp"

namespace opensu::library::roms {

std::vector<std::string> installedAndroidApps() {
    const android::ActivityCall call;
    if (!call.ready()) {
        return {};
    }
    auto* packages = static_cast<jobjectArray>(call.env()->CallObjectMethod(
        call.activity(), call.method("installedApps", "()[Ljava/lang/String;")));
    if (!call.takeException().empty() || packages == nullptr) {
        return {};
    }
    return call.strings(packages);
}

} // namespace opensu::library::roms

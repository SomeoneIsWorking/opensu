#include "content_insets.hpp"

#include <cstdint>

#include "activity_call.hpp"
#include "lucent/log.h"

namespace opensu::app {
namespace {

float edge(std::uint64_t packed, unsigned shift) {
    return static_cast<float>((packed >> shift) & 0xFFFFU);
}

} // namespace

gpu2d::ui::Insets contentInsets(SDL_Window* /*window*/) {
    const android::ActivityCall call;
    if (!call.ready()) {
        return {};
    }
    const jmethodID read = call.method("contentInsets", "()J");
    if (read == nullptr) {
        lucent::warn("shell", "the Activity has no contentInsets: {}", call.takeException());
        return {};
    }
    const jlong packed = call.env()->CallLongMethod(call.activity(), read);
    if (const std::string failure = call.takeException(); !failure.empty()) {
        lucent::warn("shell", "the content insets cannot be read: {}", failure);
        return {};
    }
    const auto bits = static_cast<std::uint64_t>(packed);
    return gpu2d::ui::Insets{.left = edge(bits, 0U),
                             .top = edge(bits, 16U),
                             .right = edge(bits, 32U),
                             .bottom = edge(bits, 48U)};
}

} // namespace opensu::app

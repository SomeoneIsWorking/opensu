// Which Vulkan debug messages count as validation errors.
#include "vulkan_device.hpp"

#include <cstdio>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::isValidationError;
using opensu::test::expect;

void classification() {
    expect(isValidationError(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT),
           "a validation layer error counts");
    expect(!isValidationError(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                              VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT),
           "a loader error about a driver with no device does not");
    expect(!isValidationError(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT),
           "a validation warning does not");
}

} // namespace

int main() try {
    classification();
    std::printf("vulkan_messages: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

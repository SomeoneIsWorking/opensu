#include "desktop_windows.hpp"

#include <algorithm>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

namespace opensu::launch {
namespace {

/// A number in a window's description, or -1 when it has none.
long numberOf(CFDictionaryRef window, CFStringRef key) {
    const auto* value = static_cast<CFNumberRef>(CFDictionaryGetValue(window, key));
    long number = -1;
    if (value == nullptr || CFNumberGetValue(value, kCFNumberLongType, &number) == 0) {
        return -1;
    }
    return number;
}

} // namespace

/// An on-screen window in the normal layer (0), where apps put their windows; menus, the Dock and
/// overlays sit in higher layers. The owner's pid needs no screen-recording permission.
bool DesktopWindows::anyOwnedBy(const std::vector<ProcessId>& pids) {
    if (pids.empty()) {
        return false;
    }
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (windows == nullptr) {
        return false;
    }
    bool found = false;
    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex index = 0; index < count && !found; ++index) {
        const auto* window = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, index));
        if (numberOf(window, kCGWindowLayer) != 0) {
            continue;
        }
        const long owner = numberOf(window, kCGWindowOwnerPID);
        found = std::ranges::find(pids, static_cast<ProcessId>(owner)) != pids.end();
    }
    CFRelease(windows);
    return found;
}

} // namespace opensu::launch

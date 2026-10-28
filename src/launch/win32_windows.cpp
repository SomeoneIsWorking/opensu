#include "win32_windows.hpp"

#include <algorithm>
#include <bit>

#include "win32.hpp"

namespace opensu::launch {
namespace {

struct Search {
    const std::vector<ProcessId>* pids;
    bool found{false};
};

/// A window the player could be shown: visible, unowned and not a tool window, as the taskbar
/// picks its buttons.
bool showable(HWND window) {
    if (IsWindowVisible(window) == 0 || GetWindow(window, GW_OWNER) != nullptr) {
        return false;
    }
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    return (style & WS_EX_TOOLWINDOW) == 0;
}

} // namespace

bool Win32Windows::anyOwnedBy(const std::vector<ProcessId>& pids) {
    if (pids.empty()) {
        return false;
    }
    Search search{.pids = &pids};
    EnumWindows(
        [](HWND window, LPARAM context) -> BOOL {
            auto& state = *std::bit_cast<Search*>(context);
            DWORD owner = 0;
            GetWindowThreadProcessId(window, &owner);
            if (std::ranges::find(*state.pids, static_cast<ProcessId>(owner)) !=
                    state.pids->end() &&
                showable(window)) {
                state.found = true;
                return FALSE;
            }
            return TRUE;
        },
        std::bit_cast<LPARAM>(&search));
    return search.found;
}

} // namespace opensu::launch

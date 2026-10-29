// desktop_windows — the desktop's top-level windows, for the handoff: a game is shown once one of
// its processes owns a window the player could switch to. One backend per OS, picked in
// CMakeLists.txt: `EnumWindows` on Windows (`win32_windows.cpp`), the window server's list on macOS
// (`quartz_windows.cpp`).
#pragma once

#include "game_windows.hpp"

namespace opensu::launch {

class DesktopWindows final : public GameWindows {
  public:
    [[nodiscard]] bool anyOwnedBy(const std::vector<ProcessId>& pids) override;
};

} // namespace opensu::launch

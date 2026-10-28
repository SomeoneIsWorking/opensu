// win32_windows — the desktop's top-level windows as Windows lists them, for the handoff: a game
// is shown once one of its processes owns a visible window the taskbar would show.
#pragma once

#include "game_windows.hpp"

namespace opensu::launch {

class Win32Windows final : public GameWindows {
  public:
    [[nodiscard]] bool anyOwnedBy(const std::vector<ProcessId>& pids) override;
};

} // namespace opensu::launch

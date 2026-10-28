// launch — whether a game has put up a window yet.
#pragma once

#include <vector>

#include "process_id.hpp"

namespace opensu::launch {

/// The windows the display shows. Implemented by session::GamescopeWindows; an interface so the
/// handoff does not depend on the display.
class GameWindows {
  public:
    virtual ~GameWindows() = default;

    /// True when a window the player could be shown belongs to one of `pids`. Called only from
    /// the thread running Handoff::start().
    [[nodiscard]] virtual bool anyOwnedBy(const std::vector<ProcessId>& pids) = 0;
};

} // namespace opensu::launch

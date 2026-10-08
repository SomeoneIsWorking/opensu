// launch — finds processes by command line and ends their whole trees.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <sys/types.h>

namespace iideck::launch {

/// Reads the process table. Steam runs every game under a reaper whose command
/// line names the app, and the game is that reaper's descendant, so a game is found
/// by its hint and ended with everything below it.
class ProcessTree {
  public:
    /// The processes whose command line contains `hint`. An empty hint matches
    /// nothing.
    [[nodiscard]] static std::vector<pid_t> matching(const std::string& hint);

    /// True when any process matches `hint`.
    [[nodiscard]] static bool anyMatches(const std::string& hint);

    /// Every process below `root`, found through parent links.
    [[nodiscard]] static std::vector<pid_t> descendants(pid_t root);

    /// The processes matching `hint` and every process below them, each tree's leaves first
    /// and its root last. This process and its children are never among them.
    [[nodiscard]] static std::vector<pid_t> treesMatching(const std::string& hint);

    /// SIGKILLs every process matching `hint` and all that descend from them, again
    /// until none is left to find. This process is never signalled. Returns how many signals were
    /// sent.
    static std::size_t killMatching(const std::string& hint);
};

} // namespace iideck::launch

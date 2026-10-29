// child_pipe — a pipe to a short-lived child whose ends close on exec, so only the child they are
// handed to holds them. One backend per OS, picked in CMakeLists.txt: `pipe2` on Linux
// (`child_pipe_linux.cpp`), `pipe` then `FD_CLOEXEC` on macOS (`child_pipe_darwin.cpp`).
#pragma once

#include <array>

namespace opensu::launch {

/// Opens a pipe into `ends` (read, write), both closed on exec. False with errno set when it
/// cannot be made.
[[nodiscard]] bool openChildPipe(std::array<int, 2>& ends);

} // namespace opensu::launch

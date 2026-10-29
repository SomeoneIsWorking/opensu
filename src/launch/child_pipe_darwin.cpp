#include "child_pipe.hpp"

#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

namespace opensu::launch {

// macOS has no pipe2; a launch's group spawns with POSIX_SPAWN_CLOEXEC_DEFAULT, so only another
// short-lived child started in between can inherit an end.
bool openChildPipe(std::array<int, 2>& ends) {
    if (pipe(ends.data()) != 0) {
        return false;
    }
    for (const int end : ends) {
        if (fcntl(end, F_SETFD, FD_CLOEXEC) != 0) {
            const int failed = errno;
            close(ends[0]);
            close(ends[1]);
            errno = failed;
            return false;
        }
    }
    return true;
}

} // namespace opensu::launch

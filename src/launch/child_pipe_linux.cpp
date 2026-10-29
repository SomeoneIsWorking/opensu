#include "child_pipe.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace opensu::launch {

bool openChildPipe(std::array<int, 2>& ends) {
    return pipe2(ends.data(), O_CLOEXEC) == 0;
}

} // namespace opensu::launch

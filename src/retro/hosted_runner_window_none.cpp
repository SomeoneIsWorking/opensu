#include "hosted_runner_window.hpp"

#include <stdexcept>

namespace opensu::retro {

std::unique_ptr<RunnerWindow> openHostedWindow() {
    throw std::runtime_error{"only Android hosts the runner in the shell's window"};
}

} // namespace opensu::retro

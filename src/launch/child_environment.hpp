// launch — the environment a program openSU starts gets: openSU's own, less what was meant for
// openSU's process alone.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace opensu::launch {

/// Turns the Gamescope WSI Vulkan layer off. Set for the shell inside Gamescope, whose window
/// must reach Gamescope through Xwayland on its 32-bit visual: the layer gives it a Wayland
/// swapchain, which SDL makes opaque. Games keep the layer, so no child is given it.
inline constexpr std::string_view gamescopeWsiOff = "DISABLE_GAMESCOPE_WSI";

/// The `envp` for exec, built in the parent before a fork (allocating in a forked child of a
/// threaded process is not safe), owning the strings the pointers refer to.
class ChildEnvironment {
  public:
    /// `entries` is a null-terminated "NAME=value" array, as `environ` is.
    explicit ChildEnvironment(char** entries);

    [[nodiscard]] char** data() noexcept {
        return pointers_.data();
    }

  private:
    std::vector<std::string> owned_;
    std::vector<char*> pointers_;
};

} // namespace opensu::launch

// core_environment — the environment commands a core may ask: directories, pixel format, core
// options, logging, input descriptors, geometry and timing changes. Anything else is refused.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <libretro.h>

#include "core_options.hpp"

namespace opensu::retro {

class CoreEnvironment {
  public:
    CoreEnvironment(const std::filesystem::path& systemDir, const std::filesystem::path& saveDir);

    /// Answers one command as libretro's environment callback does: false when refused.
    bool answer(unsigned command, void* data);

    [[nodiscard]] retro_pixel_format pixelFormat() const {
        return pixelFormat_;
    }
    [[nodiscard]] const CoreOptions& options() const {
        return options_;
    }

    /// Timing the core changed since the last call (SET_SYSTEM_AV_INFO).
    [[nodiscard]] std::optional<retro_system_timing> takeTiming();
    /// Geometry the core changed since the last call (SET_GEOMETRY or SET_SYSTEM_AV_INFO).
    [[nodiscard]] std::optional<retro_game_geometry> takeGeometry();

  private:
    std::string systemDir_;
    std::string saveDir_;
    retro_pixel_format pixelFormat_{RETRO_PIXEL_FORMAT_0RGB1555};
    CoreOptions options_;
    std::optional<retro_system_timing> timing_;
    std::optional<retro_game_geometry> geometry_;
};

} // namespace opensu::retro

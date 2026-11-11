// output — a display's size and refresh rate, as SDL reads it.
#pragma once

#include <cstdint>
#include <optional>

namespace opensu::display {

/// A display's mode. A default (zero-sized) output is the native one: a top-level Gamescope on the
/// DRM backend takes the connector's own mode.
struct Output {
    int width{0};
    int height{0};
    /// Zero or negative leaves the refresh rate to whoever shows it.
    int refreshHz{0};
    /// The desktop's pixels per dp on it, which a nested Gamescope's windows are told
    /// (`scaleVariable`); zero where there is no desktop.
    float scale{0.0F};

    [[nodiscard]] bool native() const noexcept {
        return width <= 0 || height <= 0;
    }
};

/// The size, refresh rate and scale of SDL display `display`, with SDL's video already up. Nothing
/// when SDL cannot read its mode.
[[nodiscard]] std::optional<Output> displayOutput(std::uint32_t display);

} // namespace opensu::display

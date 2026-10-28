#include <array>

#include "game_scaling.hpp"
#include "session/root_properties.hpp"

namespace opensu::app {
namespace {

/// GamescopeUpscaleFilter and GamescopeUpscaleScaler (Gamescope's `src/main.hpp`).
constexpr long filterLinear = 0;
constexpr long filterNearest = 1;
constexpr long filterFsr = 2;
constexpr long filterNis = 3;
constexpr long scalerAuto = 0;
constexpr long scalerInteger = 1;

/// Gamescope's runtime controls on the root (`steamcompmgr.cpp` handle_property_notify).
class GamescopeScaling final : public ScalingOutput {
  public:
    GamescopeScaling() : root_{"set Gamescope's resolution and upscaling"} {
    }

    void resize(settings::ScreenSize size) override {
        // Xwayland server 0, where openSU and its games live; no size past the output.
        const std::array<std::uint32_t, 4> mode{0, static_cast<std::uint32_t>(size.width),
                                                static_cast<std::uint32_t>(size.height), 0};
        root_.setCardinals("GAMESCOPE_XWAYLAND_MODE_CONTROL", mode);
        root_.flush();
    }

    void upscale(settings::ScalingFilter filter, std::uint32_t gamescopeSharpness) override {
        long wanted = filterLinear;
        long scaler = scalerAuto;
        switch (filter) {
        case settings::ScalingFilter::Linear:
            break;
        case settings::ScalingFilter::Fsr:
            wanted = filterFsr;
            break;
        case settings::ScalingFilter::Nis:
            wanted = filterNis;
            break;
        case settings::ScalingFilter::Nearest:
            wanted = filterNearest;
            break;
        case settings::ScalingFilter::Integer:
            wanted = filterNearest;
            scaler = scalerInteger;
            break;
        }
        root_.setCardinal("GAMESCOPE_FSR_SHARPNESS", gamescopeSharpness);
        root_.setCardinal("GAMESCOPE_NEW_SCALING_SCALER", scaler);
        root_.setCardinal("GAMESCOPE_NEW_SCALING_FILTER", wanted);
        root_.flush();
    }

  private:
    session::RootProperties root_;
};

} // namespace

bool scalingOnPlatform() noexcept {
    return true;
}

std::unique_ptr<ScalingOutput> openScalingOutput(bool insideGamescope) {
    if (!insideGamescope) {
        return nullptr;
    }
    return std::make_unique<GamescopeScaling>();
}

} // namespace opensu::app

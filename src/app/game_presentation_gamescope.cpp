#include <array>

#include "game_presentation.hpp"
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

/// Gamescope's runtime controls on the root (`steamcompmgr.cpp` handle_property_notify) and the
/// fork's frame generation atoms (`src/FrameGen/`, pinned in cmake/Gamescope.cmake), all CARDINAL
/// but the DLL's path.
class GamescopePresentation final : public PresentationOutput {
  public:
    GamescopePresentation() : root_{"set Gamescope's frame generation, resolution and upscaling"} {
    }

    void generate(const FrameGenRequest& request) override {
        const settings::FrameGenSettings& on = request.settings;
        root_.setText("GAMESCOPE_FRAMEGEN_DLL", request.dll.string());
        root_.setCardinal("GAMESCOPE_FRAMEGEN_MODE",
                          on.mode == settings::FrameGenMode::Adaptive ? 1 : 0);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_RATIO", on.ratioThousandths);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_TARGET", on.targetThousandths);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_MAX", on.maxGenerated);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_TOLERANCE", on.toleranceThousandths);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_PERFORMANCE", on.performance ? 1 : 0);
        root_.setCardinal("GAMESCOPE_FRAMEGEN_FLOW_SCALE", on.flowScalePercent);
        // Last, so the generator starts once with every value in place.
        root_.setCardinal("GAMESCOPE_FRAMEGEN", 1);
        root_.flush();
    }

    void stopGenerating() override {
        root_.setCardinal("GAMESCOPE_FRAMEGEN", 0);
        root_.flush();
    }

    void resize(settings::ScreenSize size) override {
        // Xwayland server 0, where openSU and its games live; no size past the output.
        const std::array<std::uint32_t, 4> mode{0, static_cast<std::uint32_t>(size.width),
                                                static_cast<std::uint32_t>(size.height), 0};
        root_.setCardinals("GAMESCOPE_XWAYLAND_MODE_CONTROL", mode);
        root_.flush();
    }

    void upscale(const Upscale& upscale) override {
        long filter = filterLinear;
        long scaler = scalerAuto;
        switch (upscale.filter) {
        case settings::ScalingFilter::Linear:
            break;
        case settings::ScalingFilter::Fsr:
            filter = filterFsr;
            break;
        case settings::ScalingFilter::Nis:
            filter = filterNis;
            break;
        case settings::ScalingFilter::Nearest:
            filter = filterNearest;
            break;
        case settings::ScalingFilter::Integer:
            filter = filterNearest;
            scaler = scalerInteger;
            break;
        }
        root_.setCardinal("GAMESCOPE_FSR_SHARPNESS", upscale.sharpness);
        root_.setCardinal("GAMESCOPE_NEW_SCALING_SCALER", scaler);
        root_.setCardinal("GAMESCOPE_NEW_SCALING_FILTER", filter);
        root_.flush();
    }

  private:
    session::RootProperties root_;
};

} // namespace

bool presentationOnPlatform() noexcept {
    return true;
}

std::unique_ptr<PresentationOutput> openPresentationOutput(bool insideGamescope) {
    if (!insideGamescope) {
        return nullptr;
    }
    return std::make_unique<GamescopePresentation>();
}

} // namespace opensu::app

#include "frame_generation.hpp"

#include "session/root_properties.hpp"

namespace opensu::app {
namespace {

/// The Gamescope fork's frame generation atoms on the root, all CARDINAL but the DLL's path
/// (`src/FrameGen/`, pinned in cmake/Gamescope.cmake).
class GamescopeFrameGen final : public FrameGenOutput {
  public:
    GamescopeFrameGen() : root_{"set Gamescope's frame generation"} {
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

    void off() override {
        root_.setCardinal("GAMESCOPE_FRAMEGEN", 0);
        root_.flush();
    }

  private:
    session::RootProperties root_;
};

} // namespace

bool frameGenOnPlatform() noexcept {
    return true;
}

std::unique_ptr<FrameGenOutput> openFrameGenOutput(bool insideGamescope) {
    if (!insideGamescope) {
        return nullptr;
    }
    return std::make_unique<GamescopeFrameGen>();
}

} // namespace opensu::app

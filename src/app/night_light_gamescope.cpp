#include "night_light.hpp"

#include "session/night_mode.hpp"
#include "session/root_properties.hpp"

namespace opensu::app {
namespace {

/// The Gamescope fork's night mode atom on the root: amount, hue and saturation as float bits.
class GamescopeNightLight final : public NightLightOutput {
  public:
    GamescopeNightLight() : root_{"set Gamescope's night light"} {
    }

    void show(int kelvin) override {
        root_.setCardinals("GAMESCOPE_COLOR_NIGHT_MODE", session::nightModeOf(kelvin).atom());
        root_.flush();
    }

  private:
    session::RootProperties root_;
};

} // namespace

bool nightLightSupported() {
    return true;
}

std::unique_ptr<NightLightOutput> openNightLightOutput(bool loginSession, bool insideGamescope) {
    if (!loginSession || !insideGamescope) {
        return nullptr;
    }
    return std::make_unique<GamescopeNightLight>();
}

} // namespace opensu::app

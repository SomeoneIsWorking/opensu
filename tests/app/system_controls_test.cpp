// The machine's own controls as Settings rows: what the machine lacks is not offered, and each
// row's change reaches its owner and is kept.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "host_fakes.hpp"
#include "settings/settings.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "system_controls.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

class FakeProfiles final : public host::PowerProfiles {
  public:
    std::optional<host::PowerProfileState> read() override {
        return state;
    }
    std::string set(const std::string& profile) override {
        state.profile = profile;
        return {};
    }

    host::PowerProfileState state{"balanced", {"quiet", "balanced", "performance"}};
};

struct Rig {
    explicit Rig(bool handheld = true)
        : scratch{OPENSU_TEST_SCRATCH, "quick-settings"},
          preferences{settings::Store{scratch.path() / "settings.json"},
                      [](const std::string&) {
                      }},
          volume{std::make_unique<test::FakeMixer>(), {}}, brightness{handheld ? &light : nullptr},
          lights{preferences, std::nullopt, nullptr, {}},
          quick{app::SystemControls::Services{.volume = volume,
                                              .brightness = brightness,
                                              .powerProfiles = handheld ? &profiles : nullptr,
                                              .preferences = preferences,
                                              .lights = lights,
                                              .nightLight = true,
                                              .loginSession = true},
                app::SystemControls::Hooks{.applyLayout =
                                               [this] {
                                                   ++layouts;
                                               },
                                           .refused =
                                               [](const std::string&) {
                                               }}} {
        quick.read();
    }

    [[nodiscard]] bool offers(app::SystemControl control) const {
        return !quick.rows(control).empty();
    }
    [[nodiscard]] settings::Settings onDisk() const {
        return settings::Store{scratch.path() / "settings.json"}.load();
    }

    test::ScratchDir scratch;
    test::FakeBacklight light;
    FakeProfiles profiles;
    app::Preferences preferences;
    app::VolumeControl volume;
    app::BrightnessControl brightness;
    app::RingLightSettings lights;
    app::SystemControls quick;
    int layouts{0};
};

void offeredFollowsTheMachine() {
    Rig handheld;
    expect(handheld.offers(app::SystemControl::Brightness) &&
               handheld.offers(app::SystemControl::PowerProfile),
           "a backlight and PowerStation: their rows are offered");
    Rig desktop{false};
    expect(!desktop.offers(app::SystemControl::Brightness) &&
               !desktop.offers(app::SystemControl::PowerProfile) &&
               desktop.offers(app::SystemControl::Volume) &&
               desktop.offers(app::SystemControl::InterfaceSize),
           "no backlight and no PowerStation: those are left out, the rest stay");
}

void changesReachTheirOwners() {
    Rig rig;
    rig.quick.setLevel("system.volume", 70);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 70,
           "Volume sets the mixer");
    rig.quick.run("system.mute", 0);
    expect(test::need(rig.volume.system().state(), "a volume").muted, "Mute mutes");
    rig.quick.setLevel("system.brightness", 30);
    expect(rig.light.level == 30, "Brightness sets the backlight");
    rig.quick.run("system.powerProfile", 1);
    expect(rig.profiles.state.profile == "performance", "Right on Power profile steps it");
    rig.quick.setLevel("system.scale", 120);
    expect(rig.onDisk().uiScale == 120 && rig.layouts == 1,
           "Interface size is kept and the screen follows");
    rig.quick.run("system.nightLight", 1);
    expect(rig.onDisk().nightLight == settings::NightLightChoice::Off, "Night light is kept");
    if (rig.offers(app::SystemControl::Screen)) {
        rig.quick.run("system.screen", 1);
        expect(rig.onDisk().portrait && rig.layouts == 2, "Screen turns it and the screen follows");
    }
}

} // namespace

int main() try {
    offeredFollowsTheMachine();
    changesReachTheirOwners();
    std::printf("system controls: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

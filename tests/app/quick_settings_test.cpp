// Quick settings: the default pick, what the machine lacks left out, each row's change reaching its
// owner and kept, the order moved and kept through Settings › Quick settings' places, and the
// panel opening at the right edge with the picked rows and closing on Back.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "host_fakes.hpp"
#include "quick_settings.hpp"
#include "quick_settings_panel.hpp"
#include "settings/settings.hpp"
#include "support/fake_interface_text.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
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
          quick{app::QuickSettings::Services{.volume = volume,
                                             .brightness = brightness,
                                             .powerProfiles = handheld ? &profiles : nullptr,
                                             .preferences = preferences,
                                             .lights = lights,
                                             .nightLight = true,
                                             .loginSession = true},
                {.applyLayout =
                     [this] {
                         ++layouts;
                     },
                 .refused =
                     [](const std::string&) {
                     }}},
          panel{ui.navigator, quick, sounds, {.choose = [this] {
                    ++chose;
                }}} {
        quick.read();
    }

    [[nodiscard]] std::vector<std::string> shownIds() const {
        std::vector<std::string> ids;
        for (const ui::SettingsRow& row : quick.shown()) {
            ids.push_back(row.id);
        }
        return ids;
    }
    [[nodiscard]] ui::SettingsRow placeRow(const std::string& key) const {
        const ui::SettingsCategory page = quick.page();
        const auto row = std::ranges::find(page.rows, "quick.place." + key, &ui::SettingsRow::id);
        expect(row != page.rows.end(), ("the order lists " + key).c_str());
        return *row;
    }
    [[nodiscard]] settings::Settings onDisk() const {
        return settings::Store{scratch.path() / "settings.json"}.load();
    }

    test::ScratchDir scratch;
    test::InterfaceRig ui;
    test::FakeBacklight light;
    FakeProfiles profiles;
    app::Preferences preferences;
    app::VolumeControl volume;
    app::BrightnessControl brightness;
    app::RingLightSettings lights;
    audio::SdlSoundPlayer sounds;
    app::QuickSettings quick;
    app::QuickSettingsPanel panel;
    int layouts{0};
    int chose{0};
};

void defaultsFollowTheMachine() {
    Rig handheld;
    expect(handheld.shownIds() == std::vector<std::string>{"quick.volume", "quick.mute",
                                                           "quick.brightness", "quick.powerProfile",
                                                           "quick.scale"},
           "unset, the default pick shows in order");
    Rig desktop{false};
    expect(desktop.shownIds() ==
               std::vector<std::string>{"quick.volume", "quick.mute", "quick.scale"},
           "no backlight and no PowerStation: those are left out");
    const ui::SettingsCategory page = desktop.quick.page();
    expect(std::ranges::none_of(page.rows,
                                [](const ui::SettingsRow& row) {
                                    return row.id == "quick.place.brightness";
                                }),
           "nor offered to pick");
}

void changesReachTheirOwners() {
    Rig rig;
    rig.quick.setLevel("quick.volume", 70);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 70,
           "Volume sets the mixer");
    rig.quick.run("quick.mute", 0);
    expect(test::need(rig.volume.system().state(), "a volume").muted, "Mute mutes");
    rig.quick.setLevel("quick.brightness", 30);
    expect(rig.light.level == 30, "Brightness sets the backlight");
    rig.quick.run("quick.powerProfile", 1);
    expect(rig.profiles.state.profile == "performance", "Right on Power profile steps it");
    rig.quick.setLevel("quick.scale", 120);
    expect(rig.onDisk().uiScale == 120 && rig.layouts == 1,
           "Interface size is kept and the screen follows");
    rig.quick.run("quick.nightLight", 1);
    expect(rig.onDisk().nightLight == settings::NightLightChoice::Off, "Night light is kept");
}

void theOrderMovesAndKeeps() {
    Rig rig;
    expect(rig.placeRow("volume").value == "1" && rig.placeRow("nightLight").value == "Not shown",
           "the shown ones read their place, the rest Not shown");
    rig.quick.run("quick.place.nightLight", -1);
    expect(rig.shownIds().back() == "quick.nightLight" && rig.onDisk().quickSettings.has_value(),
           "Left on one not shown puts it last, kept");
    rig.quick.run("quick.place.nightLight", -5);
    expect(rig.shownIds().front() == "quick.nightLight", "picking place 1 puts it first");
    rig.quick.run("quick.place.volume", 10);
    const std::vector<std::string> ids = rig.shownIds();
    expect(std::ranges::find(ids, "quick.volume") == ids.end(),
           "past the last place it leaves the panel");
    rig.quick.run("quick.restore", 0);
    expect(!rig.onDisk().quickSettings && rig.shownIds().front() == "quick.volume",
           "Restore defaults forgets the pick");
}

void unknownSpellingsAreSkipped() {
    Rig rig;
    rig.preferences.values().quickSettings = std::vector<std::string>{"wifi", "mute", "mute"};
    expect(rig.shownIds() == std::vector<std::string>{"quick.mute"},
           "a spelling this build does not know, or one twice, is left out");
}

void thePanelOpensAtTheRightEdge() {
    Rig rig;
    rig.panel.toggle();
    rig.ui.frame(1.0);
    expect(rig.panel.isOpen() && rig.ui.navigator.focused() == "quick.volume",
           "the panel opens on its first setting");
    std::vector<std::string> ids;
    for (const ui::SettingsRow& row : rig.panel.rows()) {
        ids.push_back(row.id);
    }
    expect(ids.size() == 6 && ids.back() == "quickPanel.choose",
           "the picked rows, then Choose what shows");
    rig.ui.navigator.act(Action::Back);
    rig.ui.frame(1.0);
    expect(!rig.panel.isOpen(), "Back closes it");
    rig.panel.toggle();
    rig.ui.frame(1.0);
    rig.panel.run("quickPanel.choose", 0);
    rig.ui.frame(1.0);
    expect(!rig.panel.isOpen() && rig.chose == 1, "Choose what shows leads to Settings");
}

} // namespace

int main() try {
    defaultsFollowTheMachine();
    changesReachTheirOwners();
    theOrderMovesAndKeeps();
    unknownSpellingsAreSkipped();
    thePanelOpensAtTheRightEdge();
    std::printf("quick settings: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

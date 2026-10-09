// The volume actions over a fake mixer: each moves the mixer five percent or mutes it, every change
// is shown once, the volume at start is not a change, and a missing mixer is refused with a toast.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ui/check.hpp"
#include "volume_control.hpp"

namespace {

using namespace opensu;
using input::Action;
using test::expect;

class FakeMixer final : public audio::VolumeBackend {
  public:
    FakeMixer(audio::VolumeState* state, bool* wrote) : state_{state}, wrote_{wrote} {
    }
    [[nodiscard]] std::string_view name() const override {
        return "fake";
    }
    std::optional<audio::VolumeState> read() override {
        return *state_;
    }
    bool setPercent(int percent) override {
        *wrote_ = true;
        state_->percent = percent;
        return true;
    }
    bool setMuted(bool muted) override {
        *wrote_ = true;
        state_->muted = muted;
        return true;
    }

  private:
    audio::VolumeState* state_;
    bool* wrote_;
};

void actionsMoveTheMixerAndAreShownOnce() {
    audio::VolumeState mixer{40, false};
    bool wrote = false;
    std::vector<audio::VolumeState> shown;
    std::vector<std::string> said;
    app::VolumeControl control{std::make_unique<FakeMixer>(&mixer, &wrote),
                               {[&](const audio::VolumeState& s) {
                                    shown.push_back(s);
                                },
                                [&](const std::string& text, bool) {
                                    said.push_back(text);
                                }}};
    expect(shown.empty(), "the volume at start is not shown");
    control.act(Action::VolumeUp);
    expect(mixer.percent == 45 && shown.size() == 1 && shown[0].percent == 45, "up");
    control.act(Action::VolumeDown);
    control.act(Action::VolumeDown);
    expect(mixer.percent == 35 && shown.size() == 3, "down, each shown once");
    control.act(Action::VolumeMute);
    expect(mixer.muted && shown.back().muted, "mute");
    control.act(Action::VolumeMute);
    expect(!mixer.muted, "unmute");
    wrote = false;
    control.act(Action::Quit);
    control.act(Action::Confirm);
    expect(!wrote, "an action that is not the volume's does nothing");
    expect(said.empty(), "nothing was refused");
}

void withoutAMixerEveryActionIsRefused() {
    std::vector<std::string> said;
    std::size_t shown = 0;
    app::VolumeControl control{nullptr,
                               {[&](const audio::VolumeState&) {
                                    ++shown;
                                },
                                [&](const std::string& text, bool error) {
                                    said.push_back(error ? text : "");
                                }}};
    control.act(Action::VolumeUp);
    control.act(Action::VolumeMute);
    expect(said.size() == 2 && said[0].find("install") != std::string::npos && shown == 0,
           "each is refused with the install advice and shows nothing");
}

} // namespace

int main() {
    actionsMoveTheMixerAndAreShownOnce();
    withoutAMixerEveryActionIsRefused();
    std::printf("volume_control: all checks passed\n");
    return 0;
}

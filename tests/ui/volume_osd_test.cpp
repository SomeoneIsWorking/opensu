// The volume display's timing: shown on every change, held, then faded out.
#include "volume_osd.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using namespace opensu::ui;
using opensu::test::expect;
using Clock = VolumeOsd::Clock;

void shownHeldAndFaded() {
    VolumeOsd osd;
    const Clock::time_point start{std::chrono::seconds{100}};
    expect(!osd.visible(start) && !osd.holding(), "nothing shown at first");
    osd.show({40, false}, start);
    expect(osd.visible(start) && osd.holding() && osd.level() == VolumeLevel{40, false},
           "a change shows it");
    const auto later = start + std::chrono::milliseconds{500};
    expect(osd.look(later).alpha > 0.99f, "fully up once faded in");
    osd.tick(later);
    expect(osd.holding(), "still held inside the hold time");
    osd.show({45, false}, later);
    osd.tick(start + volumeOsdHold + std::chrono::milliseconds{100});
    expect(osd.holding() && osd.level().percent == 45,
           "a new change restarts the hold and replaces the level");
    const auto over = later + volumeOsdHold + std::chrono::milliseconds{1};
    osd.tick(over);
    expect(!osd.holding() && osd.visible(over), "after the hold it starts fading out");
    expect(osd.look(over + std::chrono::milliseconds{120}).alpha < 1.0f, "fading");
    expect(!osd.visible(over + std::chrono::seconds{2}), "and is gone");
    osd.show({0, true}, over + std::chrono::seconds{3});
    expect(osd.visible(over + std::chrono::seconds{3}) && osd.level().muted,
           "a later change shows it again, muted");
}

} // namespace

int main() {
    shownHeldAndFaded();
    std::printf("volume_osd: all checks passed\n");
    return 0;
}

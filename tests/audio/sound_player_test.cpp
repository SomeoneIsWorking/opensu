// The player on a machine that may have no audio device: it never crashes and never claims to play
// what it cannot.
#include "sound_player.hpp"

#include <cstdio>

#include "ui/check.hpp"

namespace {

using opensu::audio::Effect;
using opensu::audio::SoundPlayer;
using opensu::test::expect;

void silentUntilOpened() {
    SoundPlayer player;
    expect(!player.ready(), "a new player has no device");
    expect(!player.load(Effect::Navigation, "no-such-file.wav"), "it loads nothing unopened");
    expect(!player.play(Effect::Navigation), "it plays nothing unopened");
}

void openingNeverFails() {
    SoundPlayer player;
    player.open();
    player.open();
    expect(!player.load(Effect::Open, "no-such-file.wav"), "a missing file is refused");
    expect(!player.play(Effect::Open), "an effect without a file plays nothing");
}

} // namespace

int main() {
    silentUntilOpened();
    openingNeverFails();
    std::printf("sound_player: all checks passed\n");
    return 0;
}

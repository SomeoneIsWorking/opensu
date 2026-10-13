// A core loaded through Core: the fake core's frames, samples, input reads and save RAM reach the
// runner's owners, and load failures are CoreErrors.
#include "core.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "audio_sink.hpp"
#include "check.hpp"
#include "core_environment.hpp"
#include "joypad.hpp"
#include "scratch_dir.hpp"
#include "video_frame.hpp"

namespace {

using opensu::retro::AudioSink;
using opensu::retro::Core;
using opensu::retro::CoreEnvironment;
using opensu::retro::CoreError;
using opensu::retro::CoreHost;
using opensu::retro::Joypad;
using opensu::retro::VideoFrame;
using opensu::test::expect;
using opensu::test::fail;

template <class Action> void expectCoreError(Action action, const char* what) {
    try {
        action();
    } catch (const CoreError& failure) {
        std::printf("expected: %s\n", failure.what());
        return;
    }
    fail(what);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        fail("usage: core_test <fake core>");
    }
    const std::filesystem::path library = argv[1];
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "retro_core"};
    const std::filesystem::path rom = scratch.path() / "game.bin";
    std::ofstream{rom, std::ios::binary} << "Bgame";

    CoreEnvironment environment{scratch.path(), scratch.path()};
    VideoFrame video;
    AudioSink sink{48000, 1920};
    Joypad joypad;
    const CoreHost host{environment, video, sink, joypad};

    expectCoreError(
        [&] {
            Core missing{scratch.path() / "none.so", host};
        },
        "a missing library is a CoreError");
    {
        Core core{library, host};
        expect(core.name() == "fake", "the core names itself");
        expectCoreError(
            [&] {
                Core second{library, host};
            },
            "a second core in the process is a CoreError");
        expectCoreError(
            [&] {
                core.loadGame(scratch.path() / "none.bin");
            },
            "a missing ROM is a CoreError");
        core.loadGame(rom);
        expect(core.avInfo().timing.fps == 60.0, "the core's timing is read at load");
        expect(environment.options().value("fake_pattern") != nullptr,
               "the core's options are defined");
        sink.setCoreRate(core.avInfo().timing.sample_rate);

        opensu::gamepad::Event start;
        start.button = opensu::gamepad::Button::Start;
        start.pressed = true;
        joypad.apply(start);
        for (int frame = 0; frame < 3; ++frame) {
            core.run();
        }
        expect(video.width() == 64 && video.height() == 48, "the frame reaches the video owner");
        const auto rgba = video.rgba();
        expect(rgba[0] == 0xFF && rgba[1] == 0 && rgba[2] == 0, "the first bar is red");
        expect(sink.fill() > 0, "the samples reach the sink");
        const auto ram = core.saveRam();
        expect(ram.size() == 256, "the save RAM is the core's");
        expect(ram[0] == std::byte{3}, "the core counted three frames");
        expect(ram[4] == std::byte{1U << RETRO_DEVICE_ID_JOYPAD_START},
               "the core read Start through the joypad");
        expect(ram[6] == std::byte{'B'}, "the core was given the ROM's bytes");
    }
    Core again{library, host};
    expect(again.name() == "fake", "a core can be loaded after the last one is gone");
    std::printf("core: all checks passed\n");
    return 0;
}

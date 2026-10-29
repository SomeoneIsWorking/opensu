// The SDL UI sound player's decode path, with no audio device: WAV and OGG decode to the PCM they
// hold, an unusable file is refused, and nothing claims to play without a device.
#include "sdl_sound_player.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <vorbis/vorbisenc.h>

#include "sound_clip.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::audio::decodeSound;
using opensu::audio::Effect;
using opensu::audio::SdlSoundPlayer;
using opensu::test::expect;
using opensu::test::need;

constexpr int rate = 22050;
constexpr int frames = 2205;

void put32(std::string& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xFFU));
    }
}

void put16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xFFU));
    out.push_back(static_cast<char>(value >> 8));
}

/// A tenth of a second of 16-bit mono at 22050 Hz.
fs::path writeWav(const fs::path& dir) {
    std::string data;
    for (int i = 0; i < frames; ++i) {
        put16(data, static_cast<std::uint16_t>(static_cast<std::int16_t>(i % 200 - 100)));
    }
    std::string wav = "RIFF";
    put32(wav, static_cast<std::uint32_t>(36 + data.size()));
    wav += "WAVEfmt ";
    put32(wav, 16);
    put16(wav, 1);
    put16(wav, 1);
    put32(wav, rate);
    put32(wav, rate * 2);
    put16(wav, 2);
    put16(wav, 16);
    wav += "data";
    put32(wav, static_cast<std::uint32_t>(data.size()));
    wav += data;
    const fs::path file = dir / "open.wav";
    std::ofstream{file, std::ios::binary} << wav;
    return file;
}

void writePages(std::ofstream& out, ogg_stream_state& stream, bool flush) {
    ogg_page page;
    while ((flush ? ogg_stream_flush(&stream, &page) : ogg_stream_pageout(&stream, &page)) != 0) {
        out.write(reinterpret_cast<const char*>(page.header), page.header_len);
        out.write(reinterpret_cast<const char*>(page.body), page.body_len);
    }
}

/// A tenth of a second of a stereo 440 Hz tone at 22050 Hz, through libvorbisenc.
fs::path writeOgg(const fs::path& dir) {
    vorbis_info info;
    vorbis_info_init(&info);
    expect(vorbis_encode_init_vbr(&info, 2, rate, 0.4f) == 0, "vorbisenc takes the format");
    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_dsp_state dsp;
    vorbis_analysis_init(&dsp, &info);
    vorbis_block block;
    vorbis_block_init(&dsp, &block);
    ogg_stream_state stream;
    ogg_stream_init(&stream, 1);
    ogg_packet header;
    ogg_packet comments;
    ogg_packet codebooks;
    vorbis_analysis_headerout(&dsp, &comment, &header, &comments, &codebooks);
    ogg_stream_packetin(&stream, &header);
    ogg_stream_packetin(&stream, &comments);
    ogg_stream_packetin(&stream, &codebooks);
    const fs::path file = dir / "domino.ogg";
    std::ofstream out{file, std::ios::binary};
    writePages(out, stream, true);

    float** buffer = vorbis_analysis_buffer(&dsp, frames);
    for (int i = 0; i < frames; ++i) {
        const float sample = 0.3f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) /
                                             static_cast<float>(rate));
        buffer[0][i] = sample;
        buffer[1][i] = sample;
    }
    vorbis_analysis_wrote(&dsp, frames);
    vorbis_analysis_wrote(&dsp, 0);
    ogg_packet packet;
    while (vorbis_analysis_blockout(&dsp, &block) == 1) {
        vorbis_analysis(&block, nullptr);
        vorbis_bitrate_addblock(&block);
        while (vorbis_bitrate_flushpacket(&dsp, &packet) != 0) {
            ogg_stream_packetin(&stream, &packet);
            writePages(out, stream, false);
        }
    }
    writePages(out, stream, true);
    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return file;
}

void wavDecodes(const fs::path& wav) {
    const auto clip = need(decodeSound(wav), "the WAV decodes");
    expect(clip.spec.format == SDL_AUDIO_S16LE, "the WAV's samples are 16-bit");
    expect(clip.spec.channels == 1 && clip.spec.freq == rate, "the WAV keeps its layout");
    expect(clip.frames() == frames, "the WAV keeps every frame");
}

void oggDecodes(const fs::path& ogg) {
    const auto clip = need(decodeSound(ogg), "the OGG decodes");
    expect(clip.spec.format == SDL_AUDIO_S16LE, "the OGG decodes to 16-bit samples");
    expect(clip.spec.channels == 2 && clip.spec.freq == rate, "the OGG keeps its layout");
    expect(clip.frames() == frames, "the OGG keeps every frame");
}

void refusesBadFiles(const fs::path& dir) {
    expect(!decodeSound(dir / "missing.wav"), "a missing WAV is refused");
    const fs::path fake = dir / "fake.ogg";
    std::ofstream{fake} << "not vorbis";
    expect(!decodeSound(fake), "a file that is not Vorbis is refused");
}

void playerWithoutDevice(const fs::path& wav, const fs::path& ogg) {
    SdlSoundPlayer player;
    expect(!player.ready(), "a player that was never opened has no device");
    expect(player.load(Effect::Open, wav), "a WAV loads without a device");
    expect(player.load(Effect::DominoOne, ogg), "an OGG loads without a device");
    expect(player.loaded(Effect::Open) && player.loaded(Effect::DominoOne), "both are kept");
    expect(!player.load(Effect::Close, wav.parent_path() / "missing.wav"), "a missing file fails");
    expect(!player.loaded(Effect::Close), "a failed load leaves the effect empty");
    expect(!player.play(Effect::Open), "nothing plays without a device");
    player.setMuted(true);
    expect(player.muted() && !player.play(Effect::Open), "nothing plays muted");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "sdl-sound"};
    const fs::path wav = writeWav(scratch.path());
    const fs::path ogg = writeOgg(scratch.path());
    wavDecodes(wav);
    oggDecodes(ogg);
    refusesBadFiles(scratch.path());
    playerWithoutDevice(wav, ogg);
    std::printf("sdl_sound_player: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

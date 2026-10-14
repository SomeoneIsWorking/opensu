#include "sound_clip.hpp"

#include <cstdio>
#include <memory>
#include <string>

#include <SDL3/SDL_stdinc.h>
#include <vorbis/vorbisfile.h>

#include "lucent/log.h"

namespace opensu::audio {
namespace {

struct SdlFree {
    void operator()(Uint8* data) const noexcept {
        SDL_free(data);
    }
};

std::optional<SoundClip> decodeWav(const std::string& file) {
    SoundClip clip;
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(file.c_str(), &clip.spec, &data, &length)) {
        lucent::warn("audio", "cannot decode {}: {}", file, SDL_GetError());
        return std::nullopt;
    }
    const std::unique_ptr<Uint8, SdlFree> owned{data};
    clip.pcm.assign(data, data + length);
    return clip;
}

struct VorbisFile {
    OggVorbis_File file{};
    bool open{false};
    VorbisFile() = default;
    VorbisFile(const VorbisFile&) = delete;
    VorbisFile& operator=(const VorbisFile&) = delete;
    ~VorbisFile() {
        if (open) {
            ov_clear(&file);
        }
    }
};

std::optional<SoundClip> decodeOgg(const std::string& file) {
    VorbisFile vorbis;
    if (ov_fopen(file.c_str(), &vorbis.file) != 0) {
        lucent::warn("audio", "cannot decode {}: not an Ogg Vorbis file", file);
        return std::nullopt;
    }
    vorbis.open = true;
    const vorbis_info* info = ov_info(&vorbis.file, -1);
    if (info == nullptr || info->channels <= 0) {
        lucent::warn("audio", "cannot decode {}: no stream", file);
        return std::nullopt;
    }
    SoundClip clip;
    clip.spec = SDL_AudioSpec{SDL_AUDIO_S16LE, info->channels, static_cast<int>(info->rate)};
    constexpr int chunk = 4096;
    std::vector<char> buffer(chunk);
    int section = 0;
    for (;;) {
        // Little-endian, 16-bit, signed.
        const long read = ov_read(&vorbis.file, buffer.data(), chunk, 0, 2, 1, &section);
        if (read == 0) {
            break;
        }
        if (read < 0) {
            lucent::warn("audio", "cannot decode {}: corrupt stream", file);
            return std::nullopt;
        }
        clip.pcm.insert(clip.pcm.end(), buffer.begin(), buffer.begin() + read);
    }
    return clip;
}

} // namespace

std::size_t SoundClip::frames() const noexcept {
    const auto frameBytes = static_cast<std::size_t>(SDL_AUDIO_FRAMESIZE(spec));
    return frameBytes == 0 ? 0 : pcm.size() / frameBytes;
}

std::optional<SoundClip> decodeSound(const std::filesystem::path& file) {
    std::string extension = file.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(SDL_tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".ogg") {
        return decodeOgg(file.string());
    }
    return decodeWav(file.string());
}

} // namespace opensu::audio

#include "sound_clip.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <SDL3/SDL_iostream.h>
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

std::optional<SoundClip> decodeWav(std::string_view bytes, std::string_view name) {
    SoundClip clip;
    Uint8* data = nullptr;
    Uint32 length = 0;
    SDL_IOStream* stream = SDL_IOFromConstMem(bytes.data(), bytes.size());
    // closeio: SDL_LoadWAV_IO closes the stream, also on failure.
    if (stream == nullptr || !SDL_LoadWAV_IO(stream, true, &clip.spec, &data, &length)) {
        lucent::warn("audio", "cannot decode {}: {}", name, SDL_GetError());
        return std::nullopt;
    }
    const std::unique_ptr<Uint8, SdlFree> owned{data};
    clip.pcm.assign(data, data + length);
    return clip;
}

/// The Ogg bytes libvorbisfile reads through its callbacks.
struct MemoryReader {
    std::string_view bytes;
    std::size_t at{0};

    static std::size_t read(void* out, std::size_t size, std::size_t count, void* source) {
        auto& self = *static_cast<MemoryReader*>(source);
        const std::size_t wanted = size * count;
        const std::size_t taken = std::min(wanted, self.bytes.size() - self.at);
        std::memcpy(out, self.bytes.data() + self.at, taken);
        self.at += taken;
        return size == 0 ? 0 : taken / size;
    }

    static int seek(void* source, ogg_int64_t offset, int whence) {
        auto& self = *static_cast<MemoryReader*>(source);
        ogg_int64_t base = 0;
        if (whence == SEEK_CUR) {
            base = static_cast<ogg_int64_t>(self.at);
        } else if (whence == SEEK_END) {
            base = static_cast<ogg_int64_t>(self.bytes.size());
        }
        const ogg_int64_t target = base + offset;
        if (target < 0 || target > static_cast<ogg_int64_t>(self.bytes.size())) {
            return -1;
        }
        self.at = static_cast<std::size_t>(target);
        return 0;
    }

    static long tell(void* source) {
        return static_cast<long>(static_cast<MemoryReader*>(source)->at);
    }
};

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

std::optional<SoundClip> decodeOgg(std::string_view bytes, std::string_view name) {
    MemoryReader reader{bytes};
    const ov_callbacks callbacks{&MemoryReader::read, &MemoryReader::seek, nullptr,
                                 &MemoryReader::tell};
    VorbisFile vorbis;
    if (ov_open_callbacks(&reader, &vorbis.file, nullptr, 0, callbacks) != 0) {
        lucent::warn("audio", "cannot decode {}: not an Ogg Vorbis file", name);
        return std::nullopt;
    }
    vorbis.open = true;
    const vorbis_info* info = ov_info(&vorbis.file, -1);
    if (info == nullptr || info->channels <= 0) {
        lucent::warn("audio", "cannot decode {}: no stream", name);
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
            lucent::warn("audio", "cannot decode {}: corrupt stream", name);
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

std::optional<SoundClip> decodeSound(std::string_view bytes, std::string_view name) {
    std::string extension = std::filesystem::path{name}.extension().string();
    for (char& c : extension) {
        c = static_cast<char>(SDL_tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".ogg") {
        return decodeOgg(bytes, name);
    }
    return decodeWav(bytes, name);
}

std::optional<SoundClip> decodeSound(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        lucent::warn("audio", "cannot decode {}: cannot be read", file.string());
        return std::nullopt;
    }
    const std::string bytes{std::istreambuf_iterator<char>{in}, {}};
    return decodeSound(bytes, file.string());
}

} // namespace opensu::audio

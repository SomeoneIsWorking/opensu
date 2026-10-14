// sound_clip — a UI sound decoded to PCM: WAV through SDL_LoadWAV, OGG Vorbis through
// libvorbisfile. Needs no audio device, so tests and a silent shell decode the same way a playing
// one does.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <SDL3/SDL_audio.h>

namespace opensu::audio {

struct SoundClip {
    /// The samples' format, channels and rate, as SDL_AudioStream takes them.
    SDL_AudioSpec spec{};
    std::vector<std::uint8_t> pcm;

    /// The clip's length in sample frames.
    [[nodiscard]] std::size_t frames() const noexcept;
};

/// The WAV or OGG in `file`, by its extension; nothing (logged) when unreadable.
[[nodiscard]] std::optional<SoundClip> decodeSound(const std::filesystem::path& file);

} // namespace opensu::audio

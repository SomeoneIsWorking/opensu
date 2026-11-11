// sound_clip — a UI sound decoded to PCM from a file or from bytes: WAV through SDL_LoadWAV_IO, OGG
// Vorbis through libvorbisfile. Needs no audio device, so tests and a silent shell decode the same
// way a playing one does.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
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

/// The most PCM a clip decodes to, about six minutes of 48 kHz 16-bit stereo; a theme's files
/// are the player's, so a small file cannot unpack into a large one.
inline constexpr std::size_t decodedLimit = std::size_t{64} << 20U;

/// The WAV or OGG `bytes`, by the extension of `name`; nothing (logged) when unusable or longer
/// than `decodedLimit`.
[[nodiscard]] std::optional<SoundClip> decodeSound(std::string_view bytes, std::string_view name);

/// The WAV or OGG in `file`, by its extension; nothing (logged) when unreadable.
[[nodiscard]] std::optional<SoundClip> decodeSound(const std::filesystem::path& file);

} // namespace opensu::audio

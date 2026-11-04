// audio_codec — remote play's sound through Opus: 48 kHz stereo in 10 ms frames, coded for low
// delay; the guest decodes each packet as it comes and has Opus fill in for one that was lost.
#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::stream {

inline constexpr int audioRate = 48000;
inline constexpr int audioChannels = 2;
/// Samples per channel in one packet: 10 ms.
inline constexpr std::size_t audioFrame = 480;

class AudioEncoder {
  public:
    /// Throws `std::runtime_error` when Opus refuses.
    AudioEncoder();
    ~AudioEncoder();
    AudioEncoder(const AudioEncoder&) = delete;
    AudioEncoder& operator=(const AudioEncoder&) = delete;
    AudioEncoder(AudioEncoder&&) = delete;
    AudioEncoder& operator=(AudioEncoder&&) = delete;

    /// One frame of interleaved samples (`audioFrame` per channel) as one packet. Throws
    /// `std::runtime_error` when Opus fails.
    [[nodiscard]] std::string encode(std::span<const float> frame);

  private:
    class Codec;
    std::unique_ptr<Codec> codec_;
};

class AudioDecoder {
  public:
    /// Throws `std::runtime_error` when Opus refuses.
    AudioDecoder();
    ~AudioDecoder();
    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;
    AudioDecoder(AudioDecoder&&) = delete;
    AudioDecoder& operator=(AudioDecoder&&) = delete;

    /// `packet`'s interleaved samples into `out`; false when it is not Opus.
    bool decode(std::string_view packet, std::vector<float>& out);
    /// A frame standing in for one that was lost, into `out`.
    void conceal(std::vector<float>& out);

  private:
    class Codec;
    std::unique_ptr<Codec> codec_;
};

} // namespace opensu::stream

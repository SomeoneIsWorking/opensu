#include <stdexcept>
#include <string>

#include <opus.h>

#include "audio_codec.hpp"

namespace opensu::stream {
namespace {

/// What a stereo game's sound needs to be clear.
constexpr opus_int32 bitsPerSecond = 192000;
/// Room for the largest packet Opus makes of one frame.
constexpr std::size_t largestPacket = 4000;

[[noreturn]] void fail(const char* what, int error) {
    throw std::runtime_error{std::string{"Opus cannot "} + what + ": " + opus_strerror(error)};
}

} // namespace

class AudioEncoder::Codec {
  public:
    Codec() {
        int error = OPUS_OK;
        encoder_ = opus_encoder_create(audioRate, audioChannels,
                                       OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
        if (error != OPUS_OK) {
            fail("make an encoder", error);
        }
        opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitsPerSecond));
    }
    ~Codec() {
        opus_encoder_destroy(encoder_);
    }
    Codec(const Codec&) = delete;
    Codec& operator=(const Codec&) = delete;
    Codec(Codec&&) = delete;
    Codec& operator=(Codec&&) = delete;

    std::string encode(std::span<const float> frame) {
        std::string packet(largestPacket, '\0');
        const opus_int32 size = opus_encode_float(
            encoder_, frame.data(), static_cast<int>(frame.size() / audioChannels),
            reinterpret_cast<unsigned char*>(packet.data()), static_cast<opus_int32>(packet.size()));
        if (size < 0) {
            fail("code a frame", size);
        }
        packet.resize(static_cast<std::size_t>(size));
        return packet;
    }

  private:
    OpusEncoder* encoder_{};
};

AudioEncoder::AudioEncoder() : codec_{std::make_unique<Codec>()} {
}

AudioEncoder::~AudioEncoder() = default;

std::string AudioEncoder::encode(std::span<const float> frame) {
    return codec_->encode(frame);
}

class AudioDecoder::Codec {
  public:
    Codec() {
        int error = OPUS_OK;
        decoder_ = opus_decoder_create(audioRate, audioChannels, &error);
        if (error != OPUS_OK) {
            fail("make a decoder", error);
        }
    }
    ~Codec() {
        opus_decoder_destroy(decoder_);
    }
    Codec(const Codec&) = delete;
    Codec& operator=(const Codec&) = delete;
    Codec(Codec&&) = delete;
    Codec& operator=(Codec&&) = delete;

    bool decode(const unsigned char* packet, opus_int32 size, std::vector<float>& out) {
        out.resize(audioFrame * audioChannels);
        const int samples = opus_decode_float(decoder_, packet, size, out.data(),
                                              static_cast<int>(audioFrame), 0);
        if (samples < 0) {
            out.clear();
            return false;
        }
        out.resize(static_cast<std::size_t>(samples) * audioChannels);
        return true;
    }

  private:
    OpusDecoder* decoder_{};
};

AudioDecoder::AudioDecoder() : codec_{std::make_unique<Codec>()} {
}

AudioDecoder::~AudioDecoder() = default;

bool AudioDecoder::decode(std::string_view packet, std::vector<float>& out) {
    return codec_->decode(reinterpret_cast<const unsigned char*>(packet.data()),
                          static_cast<opus_int32>(packet.size()), out);
}

void AudioDecoder::conceal(std::vector<float>& out) {
    codec_->decode(nullptr, 0, out);
}

} // namespace opensu::stream

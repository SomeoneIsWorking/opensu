#include <stdexcept>

#include "audio_codec.hpp"

namespace opensu::stream {
namespace {

constexpr const char* missing = "this system has no Opus to code remote play's sound";

} // namespace

std::string audioCodecMissing() {
    return missing;
}

class AudioEncoder::Codec {};
class AudioDecoder::Codec {};

AudioEncoder::AudioEncoder() {
    throw std::runtime_error{missing};
}

AudioEncoder::~AudioEncoder() = default;

std::string AudioEncoder::encode(std::span<const float> /*frame*/) {
    throw std::runtime_error{missing};
}

AudioDecoder::AudioDecoder() {
    throw std::runtime_error{missing};
}

AudioDecoder::~AudioDecoder() = default;

bool AudioDecoder::decode(std::string_view /*packet*/, std::vector<float>& /*out*/) {
    throw std::runtime_error{missing};
}

void AudioDecoder::conceal(std::vector<float>& /*out*/) {
    throw std::runtime_error{missing};
}

} // namespace opensu::stream

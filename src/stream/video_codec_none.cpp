#include <stdexcept>

#include "video_codec.hpp"

namespace opensu::stream {
namespace {

constexpr const char* missing = "this openSU is built without PyroWave, remote play's video codec";

} // namespace

class VideoEncoder::Codec {};
class VideoDecoder::Codec {};

VideoEncoder::VideoEncoder() {
    throw std::runtime_error{missing};
}

VideoEncoder::~VideoEncoder() = default;

std::vector<std::string> VideoEncoder::encode(const CapturedFrame& /*frame*/,
                                              std::size_t /*budget*/) {
    throw std::runtime_error{missing};
}

VideoDecoder::VideoDecoder() {
    throw std::runtime_error{missing};
}

VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::push(int /*width*/, int /*height*/, std::string_view /*packet*/) {
    throw std::runtime_error{missing};
}

void VideoDecoder::decode(Picture& /*picture*/) {
    throw std::runtime_error{missing};
}

} // namespace opensu::stream

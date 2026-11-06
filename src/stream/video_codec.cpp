#include "video_codec.hpp"

#include "codec_backends.hpp"

namespace opensu::stream {

std::string_view nameOf(VideoCodec codec) noexcept {
    switch (codec) {
    case VideoCodec::PyroWave:
        return "pyrowave";
    case VideoCodec::H264:
        return "h264";
    case VideoCodec::Hevc:
        return "hevc";
    case VideoCodec::Av1:
        return "av1";
    }
    return "unknown";
}

VideoCodecs encodableHere() {
    const VideoCodecs pyroWave = backend::pyroWaveBuilt() ? codecBit(VideoCodec::PyroWave) : 0;
    return static_cast<VideoCodecs>(pyroWave | backend::hardwareEncoders());
}

VideoCodecs decodableHere() {
    const VideoCodecs pyroWave = backend::pyroWaveDecodes() ? codecBit(VideoCodec::PyroWave) : 0;
    return static_cast<VideoCodecs>(pyroWave | backend::hardwareDecoders());
}

std::unique_ptr<VideoEncoder> openEncoder(VideoCodec codec) {
    return codec == VideoCodec::PyroWave ? backend::openPyroWaveEncoder()
                                         : backend::openHardwareEncoder(codec);
}

std::string decoderMissing() {
    if (backend::pyroWaveBuilt() || backend::hardwareDecoders() != 0) {
        return {};
    }
    return "this openSU is built without a video decoder for remote play";
}

std::unique_ptr<VideoDecoder> openDecoder(VideoCodec codec) {
    return codec == VideoCodec::PyroWave ? backend::openPyroWaveDecoder()
                                         : backend::openHardwareDecoder(codec);
}

} // namespace opensu::stream

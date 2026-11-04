#include <stdexcept>
#include <string>

#include "codec_backends.hpp"

namespace opensu::stream::backend {

VideoCodecs hardwareEncoders() {
    return 0;
}

VideoCodecs hardwareDecoders() {
    return 0;
}

std::unique_ptr<VideoEncoder> openHardwareEncoder(VideoCodec codec) {
    throw std::runtime_error{"this openSU is built without VA-API, so it codes no " +
                             std::string{nameOf(codec)}};
}

std::unique_ptr<VideoDecoder> openHardwareDecoder(VideoCodec codec) {
    throw std::runtime_error{"this openSU is built without VA-API, so it decodes no " +
                             std::string{nameOf(codec)}};
}

} // namespace opensu::stream::backend

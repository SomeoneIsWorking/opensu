#include <stdexcept>

#include "codec_backends.hpp"

namespace opensu::stream::backend {
namespace {

constexpr const char* missing = "this openSU is built without PyroWave, remote play's video codec";

} // namespace

bool pyroWaveBuilt() noexcept {
    return false;
}

std::unique_ptr<VideoEncoder> openPyroWaveEncoder() {
    throw std::runtime_error{missing};
}

std::unique_ptr<VideoDecoder> openPyroWaveDecoder() {
    throw std::runtime_error{missing};
}

} // namespace opensu::stream::backend

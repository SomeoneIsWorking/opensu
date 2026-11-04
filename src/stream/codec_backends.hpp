// codec_backends — the two ways `video_codec` codes, each built where it can be: PyroWave
// (`pyrowave_codec.cpp`, else `pyrowave_codec_none.cpp`) and the hardware codecs through VA-API
// (`hardware_codec_vaapi.cpp`, else `hardware_codec_none.cpp`). Only `video_codec.cpp` calls them.
#pragma once

#include <memory>

#include "video_codec.hpp"

namespace opensu::stream::backend {

/// Whether this openSU is built with PyroWave.
[[nodiscard]] bool pyroWaveBuilt() noexcept;
/// Each throws `std::runtime_error` saying why it cannot open.
[[nodiscard]] std::unique_ptr<VideoEncoder> openPyroWaveEncoder();
[[nodiscard]] std::unique_ptr<VideoDecoder> openPyroWaveDecoder();

/// The hardware codecs VA-API encodes and decodes here.
[[nodiscard]] VideoCodecs hardwareEncoders();
[[nodiscard]] VideoCodecs hardwareDecoders();
/// Each throws `std::runtime_error` saying why it cannot open.
[[nodiscard]] std::unique_ptr<VideoEncoder> openHardwareEncoder(VideoCodec codec);
[[nodiscard]] std::unique_ptr<VideoDecoder> openHardwareDecoder(VideoCodec codec);

} // namespace opensu::stream::backend

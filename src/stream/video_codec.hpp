// video_codec — remote play's pictures, coded one of two ways: PyroWave (Vulkan compute wavelets,
// every frame on its own, 100 to 200 Mbit/s: a wired link) or a hardware codec through VA-API
// (AV1, HEVC or H.264, frames built on the ones before, a few tens of Mbit/s: Wi-Fi). Each frame
// is cut into packets that each go in one datagram, and on the guest decoded once all of a frame's
// packets came. A PyroWave frame that lost one is dropped alone; a hardware codec's frames are
// dropped from the loss until the next key frame, which the guest asks for. Planes are YCbCr
// 4:2:0, BT.709 full range both ways.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "screen_capture.hpp"
#include "stream_wire.hpp"

namespace opensu::stream {

/// The most bytes of coded data one datagram carries, so a sealed video datagram fits a
/// 1500-byte Ethernet frame.
inline constexpr std::size_t videoPacketBytes = 1400;
/// The frames a second the host captures and codes at.
inline constexpr int videoFramesPerSecond = 60;

/// How a frame is coded; the value is what `wire::VideoPacket::codec` carries.
enum class VideoCodec : std::uint8_t {
    PyroWave = 0,
    H264 = 1,
    Hevc = 2,
    Av1 = 3,
};
/// The hardware codecs, the one preferred first.
inline constexpr std::array hardwareCodecs{VideoCodec::Av1, VideoCodec::Hevc, VideoCodec::H264};

/// A set of codecs, bit `VideoCodec` value.
using VideoCodecs = std::uint8_t;
[[nodiscard]] constexpr VideoCodecs codecBit(VideoCodec codec) noexcept {
    return static_cast<VideoCodecs>(1U << static_cast<unsigned>(codec));
}
[[nodiscard]] constexpr bool holds(VideoCodecs codecs, VideoCodec codec) noexcept {
    return (codecs & codecBit(codec)) != 0;
}
/// "pyrowave", "h264", "hevc" or "av1".
[[nodiscard]] std::string_view nameOf(VideoCodec codec) noexcept;

/// The codecs this machine codes: PyroWave where it is built, and each hardware codec its VA-API
/// driver and libavcodec encode.
[[nodiscard]] VideoCodecs encodableHere();
/// The codecs this machine decodes, likewise.
[[nodiscard]] VideoCodecs decodableHere();

/// One coded frame: its packets in order, each at most `videoPacketBytes`.
struct CodedFrame {
    std::vector<std::string> packets;
    /// Whether it needs no frame before it.
    bool key{false};
};

class VideoEncoder {
  public:
    VideoEncoder() = default;
    virtual ~VideoEncoder() = default;
    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;
    VideoEncoder(VideoEncoder&&) = delete;
    VideoEncoder& operator=(VideoEncoder&&) = delete;

    /// `frame` coded for about `bitsPerSecond` at 60 frames a second, a key frame when `key`. Its
    /// width and height are cut to even ones. Throws `std::runtime_error` when the codec fails.
    [[nodiscard]] virtual CodedFrame encode(const CapturedFrame& frame, std::uint64_t bitsPerSecond,
                                            bool key) = 0;
};
/// An encoder for `codec`; throws `std::runtime_error` saying why there is none.
[[nodiscard]] std::unique_ptr<VideoEncoder> openEncoder(VideoCodec codec);

/// A decoded picture: straight RGBA, rows top first.
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
};

class VideoDecoder {
  public:
    VideoDecoder() = default;
    virtual ~VideoDecoder() = default;
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = delete;
    VideoDecoder& operator=(VideoDecoder&&) = delete;

    /// Takes one packet of its codec; true once its frame has all of its packets and can be
    /// decoded. A frame begun after one still missing packets drops that one.
    virtual bool push(const wire::VideoPacket& packet) = 0;
    /// Decodes the frame `push` said was whole into `picture`; false when the codec gave no
    /// picture for it. Throws `std::runtime_error` when the codec fails.
    virtual bool decode(Picture& picture) = 0;
    /// Whether every frame is coded on its own, so one may be skipped without harm.
    [[nodiscard]] virtual bool intraOnly() const noexcept = 0;
    /// Whether a loss left it waiting for a key frame, decoding nothing until one comes.
    [[nodiscard]] virtual bool awaitingKey() const noexcept = 0;
};
/// Why this build decodes no video at all; empty when it has a decoder.
[[nodiscard]] std::string decoderMissing();
/// A decoder for `codec`; throws `std::runtime_error` saying why there is none.
[[nodiscard]] std::unique_ptr<VideoDecoder> openDecoder(VideoCodec codec);

/// A 4:2:0 picture's planes, BT.709 full range, `width` and `height` even; each chroma plane is
/// half the size each way. Rows are `lumaStride` and `chromaStride` bytes apart, and chroma
/// samples `chromaStep` apart: 1 for separate planes, 2 for NV12's interleaved one (`blue` from its
/// first byte, `red` from its second).
struct YcbcrPlanes {
    std::span<const std::uint8_t> luma;
    std::span<const std::uint8_t> blue;
    std::span<const std::uint8_t> red;
    int width{0};
    int height{0};
    std::size_t lumaStride{0};
    std::size_t chromaStride{0};
    std::size_t chromaStep{1};
};
/// The RGBA of `planes`.
void rgbaFromYcbcr(const YcbcrPlanes& planes, std::vector<std::uint8_t>& rgba);

} // namespace opensu::stream

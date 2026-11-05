// video_codec — remote play's pictures, coded one of two ways: PyroWave (Vulkan compute wavelets,
// every frame on its own, 100 to 200 Mbit/s: a wired link) or a hardware codec through VA-API
// (AV1, HEVC or H.264, frames built on the ones before, a few tens of Mbit/s: Wi-Fi). Each frame
// is cut into packets that each go in one datagram, and on the guest decoded once all of a frame's
// packets came. A PyroWave frame that lost one is dropped alone; a hardware codec's frames are
// dropped from the loss until the next key frame, which the guest asks for. Planes are YCbCr
// 4:2:0, BT.709 full range both ways. A hardware decoder can leave its picture where it decoded it,
// in DMA-BUFs the guest's GPU samples, instead of reading it back.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/dmabuf_picture.hpp"
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

/// A decoded picture: straight RGBA, rows top first; or, left where a hardware decoder wrote it,
/// its planes in DMA-BUFs (`rgba` empty).
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
    std::optional<gpu2d::DmabufYcbcr> dmabuf;
};

/// Where a decoder leaves its picture: read back into `Picture::rgba`, or in DMA-BUFs where the
/// codec decodes on the GPU (a hardware codec; PyroWave reads back either way).
enum class PictureMemory : std::uint8_t {
    Cpu,
    Dmabuf,
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
    /// Decodes the frame `push` said was whole into `picture`, left in `memory`; false when the
    /// codec gave no picture for it. Throws `std::runtime_error` when the codec fails.
    virtual bool decode(Picture& picture, PictureMemory memory) = 0;
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
/// What each chroma sample, less 128, adds to red, green and blue, in 16.16 fixed point: BT.709,
/// full range.
struct ChromaFactors {
    int redFromCr{0};
    int greenFromCb{0};
    int greenFromCr{0};
    int blueFromCb{0};
};
inline constexpr ChromaFactors bt709{
    .redFromCr = 103206, .greenFromCb = 12276, .greenFromCr = 30679, .blueFromCb = 121609};

/// The RGBA of `planes`.
void rgbaFromYcbcr(const YcbcrPlanes& planes, std::vector<std::uint8_t>& rgba);
/// The same conversion on the GPU, for samples in [0, 1].
[[nodiscard]] gpu2d::YcbcrToRgb rgbFromBt709() noexcept;

} // namespace opensu::stream

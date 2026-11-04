// video_codec — remote play's pictures through PyroWave: every frame coded on its own (intra
// only), cut into packets that each go in one datagram, and on the guest put back together and
// decoded once all of a frame's packets came. A frame that lost one is never decoded; the next
// frame's first packet drops it. Planes are YCbCr 4:2:0, BT.709 full range both ways.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "screen_capture.hpp"

namespace opensu::stream {

/// The most bytes of PyroWave data one datagram carries, so a sealed video datagram fits a
/// 1500-byte Ethernet frame.
inline constexpr std::size_t videoPacketBytes = 1400;

class VideoEncoder {
  public:
    /// Opens the GPU PyroWave codes on; throws `std::runtime_error` saying why it cannot.
    VideoEncoder();
    ~VideoEncoder();
    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;
    VideoEncoder(VideoEncoder&&) = delete;
    VideoEncoder& operator=(VideoEncoder&&) = delete;

    /// `frame` coded in at most `budget` bytes, as its packets in order. Its width and height
    /// are cut to even ones. Throws `std::runtime_error` when PyroWave fails.
    [[nodiscard]] std::vector<std::string> encode(const CapturedFrame& frame, std::size_t budget);

  private:
    class Codec;
    std::unique_ptr<Codec> codec_;
};

/// A decoded picture: straight RGBA, rows top first.
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
};

class VideoDecoder {
  public:
    /// Opens the GPU PyroWave decodes on; throws `std::runtime_error` saying why it cannot.
    VideoDecoder();
    ~VideoDecoder();
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = delete;
    VideoDecoder& operator=(VideoDecoder&&) = delete;

    /// Takes one packet of a `width` by `height` frame; true once a frame has all of its packets.
    bool push(int width, int height, std::string_view packet);
    /// Decodes the frame `push` said was whole into `picture`. Throws `std::runtime_error` when
    /// PyroWave fails.
    void decode(Picture& picture);

  private:
    class Codec;
    std::unique_ptr<Codec> codec_;
};

/// A 4:2:0 picture's planes, BT.709 full range, `width` and `height` even; each chroma plane is
/// half the size each way.
struct YcbcrPlanes {
    std::span<const std::uint8_t> luma;
    std::span<const std::uint8_t> blue;
    std::span<const std::uint8_t> red;
    int width{0};
    int height{0};
};
/// The RGBA of `planes`.
void rgbaFromYcbcr(const YcbcrPlanes& planes, std::vector<std::uint8_t>& rgba);

} // namespace opensu::stream

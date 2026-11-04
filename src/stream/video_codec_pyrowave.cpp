#include "video_codec.hpp"

#include <stdexcept>
#include <utility>

#include <vulkan/vulkan.h>

#include <pyrowave/pyrowave.h>

#include "lucent/log.h"

namespace opensu::stream {
namespace {

void check(pyrowave_result result, const char* what) {
    if (result != PYROWAVE_SUCCESS) {
        throw std::runtime_error{std::string{"PyroWave cannot "} + what + " (" +
                                 std::to_string(static_cast<int>(result)) + ")"};
    }
}

/// PyroWave's Vulkan device, made once per codec.
class Device {
  public:
    Device() {
        check(pyrowave_create_default_device(&device_), "open a Vulkan device");
    }
    ~Device() {
        pyrowave_device_destroy(device_);
    }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] pyrowave_device get() const noexcept {
        return device_;
    }

  private:
    pyrowave_device device_{};
};

int even(int value) {
    return value & ~1;
}

} // namespace

class VideoEncoder::Codec {
  public:
    ~Codec() {
        drop();
    }
    Codec() = default;
    Codec(const Codec&) = delete;
    Codec& operator=(const Codec&) = delete;
    Codec(Codec&&) = delete;
    Codec& operator=(Codec&&) = delete;

    std::vector<std::string> encode(const CapturedFrame& frame, std::size_t budget) {
        const int width = even(frame.width);
        const int height = even(frame.height);
        if (encoder_ == nullptr || width != width_ || height != height_) {
            drop();
            const pyrowave_encoder_create_info info{.device = device_.get(),
                                                    .width = width,
                                                    .height = height,
                                                    .chroma = PYROWAVE_CHROMA_SUBSAMPLING_420};
            check(pyrowave_encoder_create(&info, &encoder_), "make an encoder");
            width_ = width;
            height_ = height;
            lucent::info("video", "coding {}x{} pictures", width, height);
        }
        pyrowave_cpu_buffer planes{};
        planes.data[0] = const_cast<std::uint8_t*>(frame.luma.data());
        planes.data[1] = const_cast<std::uint8_t*>(frame.chroma.data());
        planes.row_stride_in_bytes[0] = frame.stride;
        planes.row_stride_in_bytes[1] = frame.stride;
        planes.plane_size_in_bytes[0] = frame.luma.size();
        planes.plane_size_in_bytes[1] = frame.chroma.size();
        planes.width = width;
        planes.height = height;
        planes.format = PYROWAVE_CPU_BUFFER_FORMAT_NV12;
        const pyrowave_rate_control rate{.maximum_bitstream_size = budget};
        check(pyrowave_encoder_encode_cpu(encoder_, &planes, &rate), "code a picture");

        std::size_t count = 0;
        check(pyrowave_encoder_compute_num_packets(encoder_, videoPacketBytes, &count),
              "count a picture's packets");
        packets_.resize(count);
        bitstream_.resize(count * videoPacketBytes);
        std::size_t made = 0;
        check(pyrowave_encoder_packetize(encoder_, packets_.data(), videoPacketBytes, &made,
                                         bitstream_.data(), bitstream_.size()),
              "cut a picture into packets");
        std::vector<std::string> out;
        out.reserve(made);
        for (std::size_t i = 0; i < made; ++i) {
            out.emplace_back(reinterpret_cast<const char*>(bitstream_.data()) + packets_[i].offset,
                             packets_[i].size);
        }
        return out;
    }

  private:
    void drop() noexcept {
        if (encoder_ != nullptr) {
            pyrowave_encoder_destroy(encoder_);
            encoder_ = nullptr;
        }
    }

    Device device_;
    pyrowave_encoder encoder_{};
    int width_{0};
    int height_{0};
    std::vector<pyrowave_packet> packets_;
    std::vector<std::uint8_t> bitstream_;
};

VideoEncoder::VideoEncoder() : codec_{std::make_unique<Codec>()} {
}

VideoEncoder::~VideoEncoder() = default;

std::vector<std::string> VideoEncoder::encode(const CapturedFrame& frame, std::size_t budget) {
    return codec_->encode(frame, budget);
}

class VideoDecoder::Codec {
  public:
    ~Codec() {
        drop();
    }
    Codec() = default;
    Codec(const Codec&) = delete;
    Codec& operator=(const Codec&) = delete;
    Codec(Codec&&) = delete;
    Codec& operator=(Codec&&) = delete;

    bool push(int width, int height, std::string_view packet) {
        if (width < 2 || height < 2 || (width % 2) != 0 || (height % 2) != 0) {
            return false;
        }
        if (decoder_ == nullptr || width != width_ || height != height_) {
            drop();
            const pyrowave_decoder_create_info info{.device = device_.get(),
                                                    .width = width,
                                                    .height = height,
                                                    .chroma = PYROWAVE_CHROMA_SUBSAMPLING_420,
                                                    .fragment_path = false};
            check(pyrowave_decoder_create(&info, &decoder_), "make a decoder");
            width_ = width;
            height_ = height;
            lucent::info("video", "decoding {}x{} pictures", width, height);
        }
        if (pyrowave_decoder_push_packet(decoder_, packet.data(), packet.size()) !=
            PYROWAVE_SUCCESS) {
            return false;
        }
        return pyrowave_decoder_decode_is_ready(decoder_, false);
    }

    void decode(Picture& picture) {
        const auto lumaBytes = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
        const std::size_t chromaBytes = lumaBytes / 4;
        planes_.resize(lumaBytes + 2 * chromaBytes);
        pyrowave_cpu_buffer buffer{};
        buffer.data[0] = planes_.data();
        buffer.data[1] = planes_.data() + lumaBytes;
        buffer.data[2] = planes_.data() + lumaBytes + chromaBytes;
        buffer.row_stride_in_bytes[0] = static_cast<std::size_t>(width_);
        buffer.row_stride_in_bytes[1] = static_cast<std::size_t>(width_ / 2);
        buffer.row_stride_in_bytes[2] = static_cast<std::size_t>(width_ / 2);
        buffer.plane_size_in_bytes[0] = lumaBytes;
        buffer.plane_size_in_bytes[1] = chromaBytes;
        buffer.plane_size_in_bytes[2] = chromaBytes;
        buffer.width = width_;
        buffer.height = height_;
        buffer.format = PYROWAVE_CPU_BUFFER_FORMAT_YUV420P;
        check(pyrowave_decoder_decode_cpu_buffer_synchronous(decoder_, &buffer),
              "decode a picture");
        const std::span<const std::uint8_t> all{planes_};
        picture.width = width_;
        picture.height = height_;
        rgbaFromYcbcr(YcbcrPlanes{.luma = all.subspan(0, lumaBytes),
                                  .blue = all.subspan(lumaBytes, chromaBytes),
                                  .red = all.subspan(lumaBytes + chromaBytes, chromaBytes),
                                  .width = width_,
                                  .height = height_},
                      picture.rgba);
    }

  private:
    void drop() noexcept {
        if (decoder_ != nullptr) {
            pyrowave_decoder_destroy(decoder_);
            decoder_ = nullptr;
        }
    }

    Device device_;
    pyrowave_decoder decoder_{};
    int width_{0};
    int height_{0};
    std::vector<std::uint8_t> planes_;
};

VideoDecoder::VideoDecoder() : codec_{std::make_unique<Codec>()} {
}

VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::push(int width, int height, std::string_view packet) {
    return codec_->push(width, height, packet);
}

void VideoDecoder::decode(Picture& picture) {
    codec_->decode(picture);
}

} // namespace opensu::stream

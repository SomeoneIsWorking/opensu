// The hardware codecs through the NDK's AMediaCodec, decoding only: Android hosts nothing. Each
// codec's first hardware decoder, frames fed and pictures collected from its output buffers apart.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "codec_backends.hpp"
#include "lucent/log.h"
#include "predicted_frames.hpp"

namespace opensu::stream::backend {
namespace {

constexpr std::array<std::pair<VideoCodec, const char*>, 3> mimeTypes{{
    {VideoCodec::Av1, "video/av01"},
    {VideoCodec::Hevc, "video/hevc"},
    {VideoCodec::H264, "video/avc"},
}};
/// MediaCodecInfo.CodecCapabilities' colour formats.
constexpr std::int32_t colourI420 = 19;
constexpr std::int32_t colourNv12 = 21;
constexpr std::int32_t colourFlexible = 0x7F420888;
constexpr std::int64_t inputWaitUs = 16'000;

const char* mimeOf(VideoCodec codec) {
    for (const auto& [known, mime] : mimeTypes) {
        if (known == codec) {
            return mime;
        }
    }
    throw std::runtime_error{"MediaCodec does not decode " + std::string{nameOf(codec)}};
}

struct CodecDelete {
    void operator()(AMediaCodec* codec) const noexcept {
        AMediaCodec_delete(codec);
    }
};
using Codec = std::unique_ptr<AMediaCodec, CodecDelete>;
struct FormatDelete {
    void operator()(AMediaFormat* format) const noexcept {
        AMediaFormat_delete(format);
    }
};
using Format = std::unique_ptr<AMediaFormat, FormatDelete>;

/// The platform's software decoders, which cannot be relied on for 60 frames a second.
bool software(std::string_view name) {
    return name.starts_with("c2.android.") || name.starts_with("OMX.google.");
}

/// The hardware decoder of `mime`; null when there is none.
Codec hardwareDecoder(const char* mime) {
    Codec codec{AMediaCodec_createDecoderByType(mime)};
    if (!codec) {
        return nullptr;
    }
    char* name = nullptr;
    if (AMediaCodec_getName(codec.get(), &name) != AMEDIA_OK) {
        return nullptr;
    }
    const bool hardware = !software(name);
    AMediaCodec_releaseName(codec.get(), name);
    return hardware ? std::move(codec) : nullptr;
}

std::int32_t numberOf(AMediaFormat* format, const char* key, std::int32_t otherwise) {
    std::int32_t value = otherwise;
    AMediaFormat_getInt32(format, key, &value);
    return value;
}

class Decoder final : public VideoDecoder {
  public:
    explicit Decoder(VideoCodec codec) : mime_{mimeOf(codec)} {
    }

    bool push(const wire::VideoPacket& packet) override {
        size_ = {packet.width, packet.height};
        number_ = packet.frame;
        return frames_.push(packet);
    }

    bool decode(Picture& picture, PictureMemory /*memory*/) override {
        if (!codec_ || opened_ != size_) {
            open();
        }
        if (!feed(frames_.coded())) {
            frames_.lost();
            return false;
        }
        ++inFlight_;
        return collect(picture);
    }

    [[nodiscard]] bool behind() const noexcept override {
        return inFlight_ > 0;
    }

    bool collect(Picture& picture) override {
        std::optional<std::size_t> newest;
        AMediaCodecBufferInfo shown{};
        for (;;) {
            AMediaCodecBufferInfo info{};
            const ssize_t slot = AMediaCodec_dequeueOutputBuffer(codec_.get(), &info, 0);
            if (slot == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                readLayout();
                continue;
            }
            if (slot == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                continue;
            }
            if (slot == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                break;
            }
            if (slot < 0) {
                frames_.lost();
                throw std::runtime_error{"MediaCodec failed: " + std::to_string(slot)};
            }
            inFlight_ = std::max(inFlight_, 1) - 1;
            if (newest) {
                AMediaCodec_releaseOutputBuffer(codec_.get(), *newest, false);
            }
            newest = static_cast<std::size_t>(slot);
            shown = info;
        }
        if (!newest) {
            return false;
        }
        std::size_t capacity = 0;
        const std::uint8_t* bytes = AMediaCodec_getOutputBuffer(codec_.get(), *newest, &capacity);
        const bool read =
            bytes != nullptr && shown.size > 0 &&
            this->read(std::span{bytes + shown.offset, static_cast<std::size_t>(shown.size)},
                       picture);
        AMediaCodec_releaseOutputBuffer(codec_.get(), *newest, false);
        picture.frame = static_cast<std::uint32_t>(shown.presentationTimeUs);
        return read;
    }

    [[nodiscard]] bool intraOnly() const noexcept override {
        return false;
    }

    [[nodiscard]] bool awaitingKey() const noexcept override {
        return frames_.awaitingKey();
    }

  private:
    struct Size {
        int width{0};
        int height{0};

        bool operator==(const Size&) const = default;
    };
    /// Where a picture lies in an output buffer.
    struct Layout {
        std::int32_t colour{0};
        std::size_t stride{0};
        std::size_t rows{0};
        int width{0};
        int height{0};
    };

    /// Whether the decoder takes `size_`, with Android's low-latency key when `lowLatency`.
    [[nodiscard]] bool configure(bool lowLatency) {
        codec_ = hardwareDecoder(mime_);
        if (!codec_) {
            throw std::runtime_error{std::string{"this system has no hardware decoder of "} +
                                     mime_};
        }
        const Format format{AMediaFormat_new()};
        AMediaFormat_setString(format.get(), AMEDIAFORMAT_KEY_MIME, mime_);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_WIDTH, size_.width);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_HEIGHT, size_.height);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_COLOR_FORMAT, colourFlexible);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_MAX_INPUT_SIZE,
                              size_.width * size_.height);
        AMediaFormat_setInt32(format.get(), "priority", 0);
        if (lowLatency) {
            AMediaFormat_setInt32(format.get(), "low-latency", 1);
        }
        // Kirin's own switch; other decoders leave a key they do not know.
        AMediaFormat_setInt32(format.get(),
                              "vendor.hisi-ext-low-latency-video-dec.video-scene-for-low-latency-"
                              "req",
                              1);
        AMediaFormat_setInt32(format.get(),
                              "vendor.hisi-ext-low-latency-video-dec.video-scene-for-low-latency-"
                              "rdy",
                              -1);
        return AMediaCodec_configure(codec_.get(), format.get(), nullptr, nullptr, 0) == AMEDIA_OK;
    }

    void open() {
        // The NDK cannot ask for FEATURE_LowLatency, and a decoder without it fails configure.
        if (!configure(true) && !configure(false)) {
            codec_.reset();
            throw std::runtime_error{"MediaCodec cannot configure " + std::string{mime_}};
        }
        if (const media_status_t started = AMediaCodec_start(codec_.get()); started != AMEDIA_OK) {
            codec_.reset();
            throw std::runtime_error{"MediaCodec cannot start " + std::string{mime_} + ": " +
                                     std::to_string(started)};
        }
        opened_ = size_;
        layout_ = {};
        inFlight_ = 0;
        char* name = nullptr;
        if (AMediaCodec_getName(codec_.get(), &name) == AMEDIA_OK) {
            lucent::info("video", "decoding {} through {}", mime_, name);
            AMediaCodec_releaseName(codec_.get(), name);
        }
    }

    /// Gives the decoder `coded`; false when it has no room for it now.
    bool feed(const std::string& coded) {
        const ssize_t slot = AMediaCodec_dequeueInputBuffer(codec_.get(), inputWaitUs);
        if (slot < 0) {
            lucent::warn("video", "MediaCodec took no input: {}", slot);
            return false;
        }
        const auto index = static_cast<std::size_t>(slot);
        std::size_t capacity = 0;
        std::uint8_t* input = AMediaCodec_getInputBuffer(codec_.get(), index, &capacity);
        if (input == nullptr || coded.size() > capacity) {
            AMediaCodec_queueInputBuffer(codec_.get(), index, 0, 0, 0, 0);
            lucent::warn("video", "a frame of {} bytes is too large for MediaCodec's {}",
                         coded.size(), capacity);
            return false;
        }
        std::memcpy(input, coded.data(), coded.size());
        // The frame's number comes back with its picture.
        AMediaCodec_queueInputBuffer(codec_.get(), index, 0, coded.size(), number_, 0);
        return true;
    }

    void readLayout() {
        const Format format{AMediaCodec_getOutputFormat(codec_.get())};
        const int width = numberOf(format.get(), AMEDIAFORMAT_KEY_WIDTH, opened_.width);
        const int height = numberOf(format.get(), AMEDIAFORMAT_KEY_HEIGHT, opened_.height);
        layout_ = Layout{
            .colour = numberOf(format.get(), AMEDIAFORMAT_KEY_COLOR_FORMAT, 0),
            .stride =
                static_cast<std::size_t>(numberOf(format.get(), AMEDIAFORMAT_KEY_STRIDE, width)),
            .rows = static_cast<std::size_t>(numberOf(format.get(), "slice-height", height)),
            .width = opened_.width,
            .height = opened_.height,
        };
        layout_.stride = std::max(layout_.stride, static_cast<std::size_t>(opened_.width));
        layout_.rows = std::max(layout_.rows, static_cast<std::size_t>(opened_.height));
        lucent::info("video", "MediaCodec gives colour {:#x}, {}x{} in rows of {} by {}",
                     layout_.colour, layout_.width, layout_.height, layout_.stride, layout_.rows);
    }

    /// The picture in `bytes`, by the layout the decoder said; false when it says none it can read.
    bool read(std::span<const std::uint8_t> bytes, Picture& picture) const {
        const std::size_t lumaBytes = layout_.stride * layout_.rows;
        if (layout_.stride == 0 || bytes.size() < lumaBytes) {
            return false;
        }
        YcbcrPlanes planes{.luma = bytes.first(lumaBytes),
                           .width = layout_.width & ~1,
                           .height = layout_.height & ~1,
                           .lumaStride = layout_.stride};
        const std::span<const std::uint8_t> chroma = bytes.subspan(lumaBytes);
        if (layout_.colour == colourNv12) {
            planes.blue = chroma;
            planes.red = chroma.subspan(1);
            planes.chromaStride = layout_.stride;
            planes.chromaStep = 2;
        } else if (layout_.colour == colourI420) {
            const std::size_t plane = (layout_.stride / 2) * (layout_.rows / 2);
            planes.blue = chroma.first(plane);
            planes.red = chroma.subspan(plane);
            planes.chromaStride = layout_.stride / 2;
            planes.chromaStep = 1;
        } else {
            throw std::runtime_error{"MediaCodec gives pictures in colour format " +
                                     std::to_string(layout_.colour)};
        }
        picture.width = planes.width;
        picture.height = planes.height;
        picture.dmabuf.reset();
        rgbaFromYcbcr(planes, picture.rgba);
        return true;
    }

    const char* mime_;
    Codec codec_;
    PredictedFrames frames_;
    /// The size the packets say, and the one the decoder was opened for.
    Size size_;
    Size opened_;
    Layout layout_;
    std::uint32_t number_{0};
    /// Frames fed whose pictures have not come out.
    int inFlight_{0};
};

} // namespace

VideoCodecs hardwareEncoders() {
    return 0;
}

VideoCodecs hardwareDecoders() {
    VideoCodecs codecs = 0;
    for (const auto& [codec, mime] : mimeTypes) {
        if (hardwareDecoder(mime)) {
            codecs |= codecBit(codec);
        }
    }
    return codecs;
}

std::unique_ptr<VideoEncoder> openHardwareEncoder(VideoCodec codec) {
    throw std::runtime_error{"this system codes no " + std::string{nameOf(codec)}};
}

std::unique_ptr<VideoDecoder> openHardwareDecoder(VideoCodec codec) {
    return std::make_unique<Decoder>(codec);
}

} // namespace opensu::stream::backend

// The hardware codecs through libavcodec's VA-API encoders and hwaccel decoders, on the GPU's
// first DRM render node VA-API opens. Low latency: no B-frames, one frame in flight, a constant
// rate with a two-frame buffer, a key frame every five seconds or when asked.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/opt.h>
}
#include <va/va.h>

#include "codec_backends.hpp"
#include "lucent/log.h"

namespace opensu::stream::backend {
namespace {

/// A key frame at least this often, so a guest that missed one's request recovers.
constexpr int keyFrameEvery = 5 * videoFramesPerSecond;
/// The rate a running encoder is reopened for, as a share of the one it codes at.
constexpr double reopenShare = 0.2;

std::string errorText(int error) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> text{};
    av_strerror(error, text.data(), text.size());
    return text.data();
}

void check(int result, const char* what) {
    if (result < 0) {
        throw std::runtime_error{std::string{"VA-API cannot "} + what + ": " + errorText(result)};
    }
}

struct BufferUnref {
    void operator()(AVBufferRef* buffer) const noexcept {
        av_buffer_unref(&buffer);
    }
};
using Buffer = std::unique_ptr<AVBufferRef, BufferUnref>;
struct ContextFree {
    void operator()(AVCodecContext* context) const noexcept {
        avcodec_free_context(&context);
    }
};
using Context = std::unique_ptr<AVCodecContext, ContextFree>;
struct FrameFree {
    void operator()(AVFrame* frame) const noexcept {
        av_frame_free(&frame);
    }
};
using Frame = std::unique_ptr<AVFrame, FrameFree>;
struct PacketFree {
    void operator()(AVPacket* packet) const noexcept {
        av_packet_free(&packet);
    }
};
using Packet = std::unique_ptr<AVPacket, PacketFree>;

/// libavcodec's VA-API device: the first DRM render node a VA-API driver opens.
Buffer openDevice() {
    AVDictionary* options = nullptr;
    av_dict_set(&options, "connection_type", "drm", 0);
    AVBufferRef* device = nullptr;
    const int result = av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VAAPI, nullptr, options, 0);
    av_dict_free(&options);
    check(result, "open the GPU");
    return Buffer{device};
}

/// What a codec is to libavcodec and VA-API.
struct CodecNames {
    VideoCodec codec;
    const char* encoder;
    const char* decoder;
    std::vector<VAProfile> profiles;
};

std::vector<CodecNames> codecNames() {
    return {
        {VideoCodec::Av1, "av1_vaapi", "av1", {VAProfileAV1Profile0}},
        {VideoCodec::Hevc, "hevc_vaapi", "hevc", {VAProfileHEVCMain}},
        {VideoCodec::H264,
         "h264_vaapi",
         "h264",
         {VAProfileH264High, VAProfileH264Main, VAProfileH264ConstrainedBaseline}},
    };
}

CodecNames namesOf(VideoCodec codec) {
    for (CodecNames names : codecNames()) {
        if (names.codec == codec) {
            return names;
        }
    }
    throw std::runtime_error{"VA-API does not code " + std::string{nameOf(codec)}};
}

/// The hardware codecs whose profiles the driver has `entrypoints` for, and libavcodec a coder.
VideoCodecs supported(const std::vector<VAEntrypoint>& entrypoints, bool encoding) {
    Buffer device;
    try {
        device = openDevice();
    } catch (const std::runtime_error& failure) {
        lucent::info("video", "no hardware video codec: {}", failure.what());
        return 0;
    }
    VADisplay display = reinterpret_cast<AVVAAPIDeviceContext*>(
                            reinterpret_cast<AVHWDeviceContext*>(device->data)->hwctx)
                            ->display;
    std::vector<VAProfile> profiles(static_cast<std::size_t>(vaMaxNumProfiles(display)));
    int profileCount = 0;
    if (vaQueryConfigProfiles(display, profiles.data(), &profileCount) != VA_STATUS_SUCCESS) {
        return 0;
    }
    profiles.resize(static_cast<std::size_t>(profileCount));
    const auto served = [&](VAProfile profile) {
        if (std::ranges::find(profiles, profile) == profiles.end()) {
            return false;
        }
        std::vector<VAEntrypoint> offered(static_cast<std::size_t>(vaMaxNumEntrypoints(display)));
        int count = 0;
        if (vaQueryConfigEntrypoints(display, profile, offered.data(), &count) !=
            VA_STATUS_SUCCESS) {
            return false;
        }
        offered.resize(static_cast<std::size_t>(count));
        return std::ranges::any_of(entrypoints, [&offered](VAEntrypoint entrypoint) {
            return std::ranges::find(offered, entrypoint) != offered.end();
        });
    };
    VideoCodecs codecs = 0;
    for (const CodecNames& names : codecNames()) {
        const bool coder = encoding ? avcodec_find_encoder_by_name(names.encoder) != nullptr
                                    : avcodec_find_decoder_by_name(names.decoder) != nullptr;
        if (coder && std::ranges::any_of(names.profiles, served)) {
            codecs |= codecBit(names.codec);
        }
    }
    return codecs;
}

class Encoder final : public VideoEncoder {
  public:
    explicit Encoder(VideoCodec codec)
        : names_{namesOf(codec)}, device_{openDevice()},
          encoder_{avcodec_find_encoder_by_name(names_.encoder)} {
        if (encoder_ == nullptr) {
            throw std::runtime_error{std::string{"libavcodec has no "} + names_.encoder};
        }
    }

    CodedFrame encode(const CapturedFrame& frame, std::uint64_t bitsPerSecond, bool key) override {
        const int width = frame.width & ~1;
        const int height = frame.height & ~1;
        const double change =
            std::abs(static_cast<double>(bitsPerSecond) - static_cast<double>(bitsPerSecond_)) /
            static_cast<double>(std::max<std::uint64_t>(bitsPerSecond_, 1));
        if (!context_ || width != width_ || height != height_ || change > reopenShare) {
            open(width, height, bitsPerSecond);
            key = true;
        }
        Frame software{av_frame_alloc()};
        software->format = AV_PIX_FMT_NV12;
        software->width = width;
        software->height = height;
        software->data[0] = const_cast<std::uint8_t*>(frame.luma.data());
        software->data[1] = const_cast<std::uint8_t*>(frame.chroma.data());
        software->linesize[0] = static_cast<int>(frame.stride);
        software->linesize[1] = static_cast<int>(frame.stride);
        Frame surface{av_frame_alloc()};
        check(av_hwframe_get_buffer(context_->hw_frames_ctx, surface.get(), 0), "take a surface");
        check(av_hwframe_transfer_data(surface.get(), software.get(), 0), "upload a picture");
        surface->pts = pts_++;
        if (key) {
            surface->pict_type = AV_PICTURE_TYPE_I;
        }
        check(avcodec_send_frame(context_.get(), surface.get()), "code a picture");

        CodedFrame out;
        std::string data;
        Packet packet{av_packet_alloc()};
        for (;;) {
            const int got = avcodec_receive_packet(context_.get(), packet.get());
            if (got == AVERROR(EAGAIN)) {
                break;
            }
            check(got, "take a coded picture");
            data.append(reinterpret_cast<const char*>(packet->data),
                        static_cast<std::size_t>(packet->size));
            out.key = out.key || (packet->flags & AV_PKT_FLAG_KEY) != 0;
            av_packet_unref(packet.get());
        }
        for (std::size_t at = 0; at < data.size(); at += videoPacketBytes) {
            out.packets.push_back(data.substr(at, videoPacketBytes));
        }
        return out;
    }

  private:
    void open(int width, int height, std::uint64_t bitsPerSecond) {
        context_.reset();
        Buffer frames{av_hwframe_ctx_alloc(device_.get())};
        auto* pool = reinterpret_cast<AVHWFramesContext*>(frames->data);
        pool->format = AV_PIX_FMT_VAAPI;
        pool->sw_format = AV_PIX_FMT_NV12;
        pool->width = width;
        pool->height = height;
        pool->initial_pool_size = 4;
        check(av_hwframe_ctx_init(frames.get()), "make surfaces");

        Context context{avcodec_alloc_context3(encoder_)};
        context->width = width;
        context->height = height;
        context->time_base = AVRational{1, videoFramesPerSecond};
        context->framerate = AVRational{videoFramesPerSecond, 1};
        context->pix_fmt = AV_PIX_FMT_VAAPI;
        context->sw_pix_fmt = AV_PIX_FMT_NV12;
        context->hw_frames_ctx = av_buffer_ref(frames.get());
        context->gop_size = keyFrameEvery;
        context->max_b_frames = 0;
        const auto rate = static_cast<std::int64_t>(bitsPerSecond);
        context->bit_rate = rate;
        context->rc_max_rate = rate;
        context->rc_buffer_size = static_cast<int>(rate / (videoFramesPerSecond / 2));
        context->color_range = AVCOL_RANGE_JPEG;
        context->colorspace = AVCOL_SPC_BT709;
        context->color_primaries = AVCOL_PRI_BT709;
        context->color_trc = AVCOL_TRC_BT709;
        av_opt_set(context->priv_data, "rc_mode", "CBR", 0);
        av_opt_set_int(context->priv_data, "async_depth", 1, 0);
        check(avcodec_open2(context.get(), encoder_, nullptr), "open its encoder");
        context_ = std::move(context);
        width_ = width;
        height_ = height;
        bitsPerSecond_ = bitsPerSecond;
        lucent::info("video", "coding {}x{} pictures with {} at {:.1f} Mbit/s", width, height,
                     names_.encoder, static_cast<double>(bitsPerSecond) / 1e6);
    }

    CodecNames names_;
    Buffer device_;
    const AVCodec* encoder_;
    Context context_;
    int width_{0};
    int height_{0};
    std::uint64_t bitsPerSecond_{0};
    std::int64_t pts_{0};
};

AVPixelFormat vaapiFormat(AVCodecContext* /*context*/, const AVPixelFormat* formats) {
    for (const AVPixelFormat* format = formats; *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == AV_PIX_FMT_VAAPI) {
            return *format;
        }
    }
    return AV_PIX_FMT_NONE;
}

class Decoder final : public VideoDecoder {
  public:
    explicit Decoder(VideoCodec codec) : names_{namesOf(codec)}, device_{openDevice()} {
        const AVCodec* decoder = avcodec_find_decoder_by_name(names_.decoder);
        if (decoder == nullptr) {
            throw std::runtime_error{std::string{"libavcodec has no "} + names_.decoder +
                                     " decoder"};
        }
        context_.reset(avcodec_alloc_context3(decoder));
        context_->hw_device_ctx = av_buffer_ref(device_.get());
        context_->get_format = vaapiFormat;
        context_->thread_count = 1;
        context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        check(avcodec_open2(context_.get(), decoder, nullptr), "open its decoder");
        lucent::info("video", "decoding {} through VA-API", names_.decoder);
    }

    bool push(const wire::VideoPacket& packet) override {
        if (packet.frame < frame_) {
            return false;
        }
        if (packet.frame != frame_) {
            // A frame lost on the way leaves the ones after it nothing to build on.
            if (frame_ != 0 && (!whole_ || packet.frame != frame_ + 1)) {
                awaitingKey_ = true;
            }
            frame_ = packet.frame;
            expected_ = packet.parts;
            key_ = packet.key;
            whole_ = false;
            parts_.clear();
        }
        parts_[packet.part] = packet.data;
        if (whole_ || parts_.size() != expected_) {
            return false;
        }
        whole_ = true;
        if (awaitingKey_ && !key_) {
            return false;
        }
        awaitingKey_ = false;
        return true;
    }

    bool decode(Picture& picture) override {
        coded_.clear();
        for (const auto& [part, data] : parts_) {
            coded_ += data;
        }
        Packet packet{av_packet_alloc()};
        check(av_new_packet(packet.get(), static_cast<int>(coded_.size())), "take a packet");
        std::memcpy(packet->data, coded_.data(), coded_.size());
        if (const int sent = avcodec_send_packet(context_.get(), packet.get()); sent < 0) {
            awaitingKey_ = true;
            check(sent, "decode a picture");
        }
        Frame surface{av_frame_alloc()};
        const int got = avcodec_receive_frame(context_.get(), surface.get());
        if (got == AVERROR(EAGAIN)) {
            return false;
        }
        if (got < 0) {
            awaitingKey_ = true;
            check(got, "decode a picture");
        }
        Frame planes{av_frame_alloc()};
        planes->format = AV_PIX_FMT_NV12;
        check(av_hwframe_transfer_data(planes.get(), surface.get(), 0), "read a picture back");
        const int width = planes->width & ~1;
        const int height = planes->height & ~1;
        const auto lumaStride = static_cast<std::size_t>(planes->linesize[0]);
        const auto chromaStride = static_cast<std::size_t>(planes->linesize[1]);
        const std::span<const std::uint8_t> luma{planes->data[0],
                                                 lumaStride * static_cast<std::size_t>(height)};
        const std::span<const std::uint8_t> chroma{
            planes->data[1], chromaStride * static_cast<std::size_t>(height / 2)};
        picture.width = width;
        picture.height = height;
        rgbaFromYcbcr(YcbcrPlanes{.luma = luma,
                                  .blue = chroma,
                                  .red = chroma.subspan(1),
                                  .width = width,
                                  .height = height,
                                  .lumaStride = lumaStride,
                                  .chromaStride = chromaStride,
                                  .chromaStep = 2},
                      picture.rgba);
        return true;
    }

    [[nodiscard]] bool intraOnly() const noexcept override {
        return false;
    }

    [[nodiscard]] bool awaitingKey() const noexcept override {
        return awaitingKey_;
    }

  private:
    CodecNames names_;
    Buffer device_;
    Context context_;
    /// The frame whose packets are coming, by part.
    std::uint32_t frame_{0};
    std::size_t expected_{0};
    bool key_{false};
    bool whole_{false};
    std::map<std::uint16_t, std::string> parts_;
    /// A frame was lost or failed: nothing decodes until a key frame.
    bool awaitingKey_{true};
    std::string coded_;
};

} // namespace

VideoCodecs hardwareEncoders() {
    return supported({VAEntrypointEncSlice, VAEntrypointEncSliceLP}, true);
}

VideoCodecs hardwareDecoders() {
    return supported({VAEntrypointVLD}, false);
}

std::unique_ptr<VideoEncoder> openHardwareEncoder(VideoCodec codec) {
    return std::make_unique<Encoder>(codec);
}

std::unique_ptr<VideoDecoder> openHardwareDecoder(VideoCodec codec) {
    return std::make_unique<Decoder>(codec);
}

} // namespace opensu::stream::backend

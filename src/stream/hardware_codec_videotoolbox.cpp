// The hardware codecs through VideoToolbox, decoding only: a Mac hosts nothing. HEVC and H.264
// from the host's Annex B stream: each key frame's parameter sets make the format description, the
// rest goes in as length-prefixed NAL units, and the hardware decoder converts to BGRA, which is
// read back as RGBA.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include "codec_backends.hpp"
#include "lucent/log.h"
#include "predicted_frames.hpp"

namespace opensu::stream::backend {
namespace {

/// The bytes before each NAL unit in a sample: its length, big-endian.
constexpr int lengthBytes = 4;

struct CfRelease {
    void operator()(CFTypeRef object) const noexcept {
        CFRelease(object);
    }
};
template <typename T> using Cf = std::unique_ptr<std::remove_pointer_t<T>, CfRelease>;

struct SessionRelease {
    void operator()(VTDecompressionSessionRef session) const noexcept {
        VTDecompressionSessionInvalidate(session);
        CFRelease(session);
    }
};
using Session = std::unique_ptr<std::remove_pointer_t<VTDecompressionSessionRef>, SessionRelease>;

CMVideoCodecType typeOf(VideoCodec codec) {
    switch (codec) {
    case VideoCodec::Hevc:
        return kCMVideoCodecType_HEVC;
    case VideoCodec::H264:
        return kCMVideoCodecType_H264;
    case VideoCodec::Av1:
    case VideoCodec::PyroWave:
        break;
    }
    throw std::runtime_error{"VideoToolbox does not decode " + std::string{nameOf(codec)} +
                             " here"};
}

/// The NAL units of an Annex B stream, start codes cut off.
std::vector<std::span<const std::uint8_t>> nalUnits(std::span<const std::uint8_t> stream) {
    std::vector<std::span<const std::uint8_t>> units;
    std::size_t start = 0;
    bool inUnit = false;
    for (std::size_t i = 0; i + 2 < stream.size(); ++i) {
        if (stream[i] != 0 || stream[i + 1] != 0 || stream[i + 2] != 1) {
            continue;
        }
        if (inUnit) {
            std::size_t end = i;
            // A four-byte start code's leading zero belongs to it, not the unit before.
            if (end > start && stream[end - 1] == 0) {
                --end;
            }
            units.push_back(stream.subspan(start, end - start));
        }
        start = i + 3;
        inUnit = true;
        i += 2;
    }
    if (inUnit && start < stream.size()) {
        units.push_back(stream.subspan(start));
    }
    return units;
}

/// HEVC's VPS, SPS and PPS (types 32 to 34); H.264's SPS and PPS (7 and 8).
bool parameterSet(VideoCodec codec, std::span<const std::uint8_t> unit) {
    if (unit.empty()) {
        return false;
    }
    if (codec == VideoCodec::Hevc) {
        const unsigned type = (unit[0] >> 1U) & 0x3FU;
        return type >= 32 && type <= 34;
    }
    const unsigned type = unit[0] & 0x1FU;
    return type == 7 || type == 8;
}

/// An access unit delimiter, which a sample does not carry.
bool delimiter(VideoCodec codec, std::span<const std::uint8_t> unit) {
    if (unit.empty()) {
        return false;
    }
    return codec == VideoCodec::Hevc ? ((unit[0] >> 1U) & 0x3FU) == 35 : (unit[0] & 0x1FU) == 9;
}

class Decoder final : public VideoDecoder {
  public:
    explicit Decoder(VideoCodec codec) : codec_{codec} {
        static_cast<void>(typeOf(codec));
    }
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(Decoder&&) = delete;
    ~Decoder() override = default;

    bool push(const wire::VideoPacket& packet) override {
        return frames_.push(packet);
    }

    bool decode(Picture& picture, PictureMemory /*memory*/) override {
        const std::string coded = frames_.coded();
        const std::span bytes{reinterpret_cast<const std::uint8_t*>(coded.data()), coded.size()};
        std::vector<std::span<const std::uint8_t>> sets;
        std::vector<std::uint8_t> sample;
        for (const std::span<const std::uint8_t> unit : nalUnits(bytes)) {
            if (parameterSet(codec_, unit)) {
                sets.push_back(unit);
            } else if (!delimiter(codec_, unit) && !unit.empty()) {
                const auto size = static_cast<std::uint32_t>(unit.size());
                for (int shift = (lengthBytes - 1) * 8; shift >= 0; shift -= 8) {
                    sample.push_back(
                        static_cast<std::uint8_t>(size >> static_cast<unsigned>(shift)));
                }
                sample.insert(sample.end(), unit.begin(), unit.end());
            }
        }
        if (!sets.empty()) {
            describe(sets);
        }
        if (!session_ || sample.empty()) {
            frames_.lost();
            return false;
        }
        return decodeSample(sample, picture);
    }

    [[nodiscard]] bool intraOnly() const noexcept override {
        return false;
    }

    [[nodiscard]] bool awaitingKey() const noexcept override {
        return frames_.awaitingKey();
    }

    [[nodiscard]] std::string_view backend() const noexcept override {
        return "VideoToolbox";
    }

  private:
    /// A format description from a key frame's parameter sets, and a session for it when the one
    /// open cannot take it.
    void describe(const std::vector<std::span<const std::uint8_t>>& sets) {
        std::vector<const std::uint8_t*> pointers;
        std::vector<std::size_t> sizes;
        for (const std::span<const std::uint8_t> set : sets) {
            pointers.push_back(set.data());
            sizes.push_back(set.size());
        }
        CMVideoFormatDescriptionRef made = nullptr;
        const OSStatus status = codec_ == VideoCodec::Hevc
                                    ? CMVideoFormatDescriptionCreateFromHEVCParameterSets(
                                          kCFAllocatorDefault, pointers.size(), pointers.data(),
                                          sizes.data(), lengthBytes, nullptr, &made)
                                    : CMVideoFormatDescriptionCreateFromH264ParameterSets(
                                          kCFAllocatorDefault, pointers.size(), pointers.data(),
                                          sizes.data(), lengthBytes, &made);
        if (status != noErr) {
            throw std::runtime_error{"VideoToolbox cannot describe the stream's parameter sets: " +
                                     std::to_string(status)};
        }
        Cf<CMVideoFormatDescriptionRef> format{made};
        if (session_ && VTDecompressionSessionCanAcceptFormatDescription(session_.get(), made)) {
            format_ = std::move(format);
            return;
        }
        open(made);
        format_ = std::move(format);
    }

    void open(CMVideoFormatDescriptionRef format) {
        session_.reset();
        const Cf<CFMutableDictionaryRef> specification{
            CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks)};
        CFDictionarySetValue(specification.get(),
                             kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
                             kCFBooleanTrue);
        const std::int32_t bgra = kCVPixelFormatType_32BGRA;
        const Cf<CFNumberRef> pixelFormat{
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bgra)};
        const Cf<CFMutableDictionaryRef> destination{
            CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks)};
        CFDictionarySetValue(destination.get(), kCVPixelBufferPixelFormatTypeKey,
                             pixelFormat.get());
        const VTDecompressionOutputCallbackRecord callback{.decompressionOutputCallback = &decoded,
                                                           .decompressionOutputRefCon = this};
        VTDecompressionSessionRef made = nullptr;
        const OSStatus status = VTDecompressionSessionCreate(
            kCFAllocatorDefault, format, specification.get(), destination.get(), &callback, &made);
        if (status != noErr) {
            throw std::runtime_error{"VideoToolbox has no hardware decoder of " +
                                     std::string{nameOf(codec_)} + ": " + std::to_string(status)};
        }
        session_.reset(made);
        const CMVideoDimensions size = CMVideoFormatDescriptionGetDimensions(format);
        lucent::info("video", "VideoToolbox decodes {} at {}x{}", nameOf(codec_), size.width,
                     size.height);
    }

    bool decodeSample(std::vector<std::uint8_t>& bytes, Picture& picture) {
        CMBlockBufferRef blockMade = nullptr;
        if (CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, bytes.size(),
                                               kCFAllocatorDefault, nullptr, 0, bytes.size(), 0,
                                               &blockMade) != kCMBlockBufferNoErr) {
            throw std::runtime_error{"VideoToolbox: no block buffer for a frame"};
        }
        const Cf<CMBlockBufferRef> block{blockMade};
        CMBlockBufferReplaceDataBytes(bytes.data(), block.get(), 0, bytes.size());
        CMSampleBufferRef sampleMade = nullptr;
        const std::size_t size = bytes.size();
        if (CMSampleBufferCreateReady(kCFAllocatorDefault, block.get(), format_.get(), 1, 0,
                                      nullptr, 1, &size, &sampleMade) != noErr) {
            throw std::runtime_error{"VideoToolbox: no sample buffer for a frame"};
        }
        const Cf<CMSampleBufferRef> sample{sampleMade};
        status_ = noErr;
        image_.reset();
        VTDecodeInfoFlags info = 0;
        // No flags: decoded before the call returns, through `decoded`.
        const OSStatus status =
            VTDecompressionSessionDecodeFrame(session_.get(), sample.get(), 0, nullptr, &info);
        if (status != noErr || status_ != noErr || !image_) {
            lucent::warn("video", "VideoToolbox did not decode a frame: {} {}", status, status_);
            frames_.lost();
            return false;
        }
        read(picture);
        return true;
    }

    static void decoded(void* self, void* /*frame*/, OSStatus status, VTDecodeInfoFlags /*flags*/,
                        CVImageBufferRef image, CMTime /*shown*/, CMTime /*lasts*/) {
        auto* decoder = static_cast<Decoder*>(self);
        decoder->status_ = status;
        if (status == noErr && image != nullptr) {
            decoder->image_.reset(CVPixelBufferRetain(image));
        }
    }

    /// The BGRA picture in `image_`, as RGBA rows top first.
    void read(Picture& picture) {
        CVPixelBufferRef image = image_.get();
        CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
        const auto* base = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddress(image));
        const std::size_t stride = CVPixelBufferGetBytesPerRow(image);
        const auto width = static_cast<int>(CVPixelBufferGetWidth(image)) & ~1;
        const auto height = static_cast<int>(CVPixelBufferGetHeight(image)) & ~1;
        picture.width = width;
        picture.height = height;
        picture.dmabuf.reset();
        picture.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        std::uint8_t* out = picture.rgba.data();
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* row = base + (static_cast<std::size_t>(y) * stride);
            for (int x = 0; x < width; ++x) {
                const std::uint8_t* bgra = row + (static_cast<std::size_t>(x) * 4);
                out[0] = bgra[2];
                out[1] = bgra[1];
                out[2] = bgra[0];
                out[3] = 0xFF;
                out += 4;
            }
        }
        CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    }

    VideoCodec codec_;
    PredictedFrames frames_;
    Cf<CMVideoFormatDescriptionRef> format_;
    Session session_;
    /// What the output callback gave for the frame being decoded.
    OSStatus status_{noErr};
    Cf<CVPixelBufferRef> image_;
};

} // namespace

VideoCodecs hardwareEncoders() {
    return 0;
}

VideoCodecs hardwareDecoders() {
    VideoCodecs codecs = 0;
    for (const VideoCodec codec : {VideoCodec::Hevc, VideoCodec::H264}) {
        if (VTIsHardwareDecodeSupported(typeOf(codec))) {
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

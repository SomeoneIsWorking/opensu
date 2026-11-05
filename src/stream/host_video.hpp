// host_video — the host's pictures for one session: each frame its Gamescope composes, coded as it
// arrives and sent to every guest of the audience a packet per datagram, every packet carrying when
// the frame was captured, coded and sent. Each guest's link plans its codec and rate
// (`HostStream::plan`); the guests planned onto one codec share one encode of it at the lowest of
// their rates, so a slow guest on a hardware codec does not take PyroWave from the others. A frame
// is numbered once, the same in every encode. Frames before a guest joined and its link was probed
// are not sent to it. A codec that cannot open is refused to its guests' links, which plan another;
// a key frame goes out when a codec's guests change or one of them asks. The newest frame is kept,
// so the control channel can show what went out.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "audience.hpp"
#include "host_stream.hpp"
#include "screen_capture.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

class HostVideo {
  public:
    /// Sends what the Gamescope at `gamescopeSocket` shows to `audience`, which outlives this.
    /// Throws `std::runtime_error` when the screen cannot be captured.
    HostVideo(Audience& audience, const std::filesystem::path& gamescopeSocket);
    ~HostVideo() = default;
    HostVideo(const HostVideo&) = delete;
    HostVideo& operator=(const HostVideo&) = delete;
    HostVideo(HostVideo&&) = delete;
    HostVideo& operator=(HostVideo&&) = delete;

    /// The newest frame sent and its number; nothing before the first.
    [[nodiscard]] std::optional<std::pair<std::uint32_t, Picture>> lastSent() const;

  private:
    /// One codec's encoder, the streams it coded for last and at what rate.
    struct Encoding {
        std::unique_ptr<VideoEncoder> encoder;
        std::vector<const HostStream*> guests;
        std::uint64_t bitsPerSecond{0};
    };
    /// The guests planned onto one codec, and the lowest of their rates.
    struct Group {
        std::vector<HostStream*> guests;
        std::uint64_t bitsPerSecond{0};
    };

    void send(const CapturedFrame& frame);
    /// Codes `frame` as `number` for `group` with `codec` and sends it to each of them; false when
    /// nothing went.
    bool code(const CapturedFrame& frame, VideoCodec codec, const Group& group,
              std::uint32_t number);

    Audience& audience_;
    /// Capture thread only.
    std::map<VideoCodec, Encoding> encodings_;
    std::uint32_t frames_{0};
    /// The newest frame sent, its planes as captured.
    mutable std::mutex lastMutex_;
    std::uint32_t lastNumber_{0};
    int lastWidth_{0};
    int lastHeight_{0};
    std::size_t lastStride_{0};
    std::vector<std::uint8_t> last_;
    /// Last, so it stops before what it uses goes.
    ScreenCapture capture_;
};

} // namespace opensu::stream

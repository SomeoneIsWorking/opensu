// host_video — the host's pictures for one session: each frame its Gamescope composes, coded as it
// arrives with the codec and rate the link plans (`HostStream::plan`) and sent to the guest a
// packet per datagram, every packet carrying when the frame was captured, coded and sent. Frames
// before the guest joined and the link was probed are not sent. A codec that cannot open is
// refused to the link, which plans another; a key frame goes out when the codec changes or the
// guest asks. The newest frame is kept, so the control channel can show what went out.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "host_stream.hpp"
#include "screen_capture.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

class HostVideo {
  public:
    /// Sends what the Gamescope at `gamescopeSocket` shows over `stream`, which outlives this.
    /// Throws `std::runtime_error` when the screen cannot be captured or coded.
    HostVideo(HostStream& stream, const std::filesystem::path& gamescopeSocket);
    ~HostVideo() = default;
    HostVideo(const HostVideo&) = delete;
    HostVideo& operator=(const HostVideo&) = delete;
    HostVideo(HostVideo&&) = delete;
    HostVideo& operator=(HostVideo&&) = delete;

    /// The newest frame sent and its number; nothing before the first.
    [[nodiscard]] std::optional<std::pair<std::uint32_t, Picture>> lastSent() const;

  private:
    void send(const CapturedFrame& frame);

    HostStream& stream_;
    /// The encoder of the codec planned last.
    std::unique_ptr<VideoEncoder> encoder_;
    VideoCodec codec_{VideoCodec::PyroWave};
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

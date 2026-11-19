// guest_video — the guest's pictures for one session: the host's packets queued as they come and
// fed to the decoder of their codec on a thread of its own, so a slow decode never holds up the
// stream's receiving; each frame is decoded once all of its packets are in and handed to the loop,
// or to a thread waiting for it, newest first. A frame still missing packets when the next one
// starts is dropped; for a codec whose frames build on each other the guest then wants a key frame,
// and on PyroWave a whole frame the decoder reaches only after two newer ones began arriving is
// skipped (late). Each shown frame's way from the host's capture to this screen is timed by stage,
// on this clock
// (`hostAheadUs` moves the host's times over). A hardware codec's pictures stay in the decoder's
// DMA-BUFs while the screen can sample them, and are read back once it says it cannot.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "frame_hold.hpp"
#include "guest_fit.hpp"
#include "stream_wire.hpp"
#include "video_codec.hpp"

namespace opensu::stream {

/// How long a stream goes without a picture before the guest warns that it shows none.
inline constexpr std::chrono::seconds silenceWarned{5};

class GuestVideo {
  public:
    /// Starts the decoding thread, its pictures left in `memory`. A codec's decoder opens with its
    /// first packet.
    explicit GuestVideo(PictureMemory memory);
    ~GuestVideo() = default;
    GuestVideo(const GuestVideo&) = delete;
    GuestVideo& operator=(const GuestVideo&) = delete;
    GuestVideo(GuestVideo&&) = delete;
    GuestVideo& operator=(GuestVideo&&) = delete;

    /// Queues one packet of the host's, which came at `receivedUs` while the host's clock was
    /// `hostAheadUs` ahead of this one; whether a key frame is wanted (asked until one decodes).
    /// On the stream's thread.
    bool take(const wire::VideoPacket& packet, std::uint64_t receivedUs, std::int64_t hostAheadUs);

    /// The newest decoded frame not taken yet, with when the host captured it and when it was
    /// decoded, on this clock. Any thread.
    struct Frame {
        std::uint32_t number{0};
        Picture picture;
        std::uint64_t capturedUs{0};
        std::uint64_t decodedUs{0};
    };
    /// The oldest decoded frame not yet taken (`FrameHold`). Any thread.
    [[nodiscard]] std::optional<Frame> takeFrame();
    /// As `takeFrame`, waiting for a frame to be decoded; nothing once `stop` is asked. Any
    /// thread.
    [[nodiscard]] std::optional<Frame> waitFrame(const std::stop_token& stop);
    /// The frame showing frame `number` was submitted at `submittedUs`, into the window when
    /// `drawn` (a hidden window has no surface). Any thread.
    void presented(std::uint32_t number, std::uint64_t submittedUs, bool drawn);
    /// The loop presented into the window, a new frame in it or not. Any thread.
    void passed();
    /// The newest frame decoded, taken or not, and its number; nothing before the first.
    [[nodiscard]] std::optional<Frame> lastDecoded() const;
    /// Decodes into CPU memory from the next frame on: the screen could not sample a DMA-BUF
    /// picture. Any thread.
    void readBack();

    /// How the frames went: counts, and the 50th and 95th percentile of each stage in
    /// milliseconds, as one line for the control channel.
    [[nodiscard]] std::string summary() const;
    /// This guest lost or skipped too many PyroWave frames to keep it (`GuestFit`). Any thread.
    [[nodiscard]] bool pyroWaveGivenUp() const noexcept {
        return pyroWaveGivenUp_.load();
    }
    /// The frames a second this guest keeps up with, 0 for all (`GuestFit`). Any thread.
    [[nodiscard]] std::uint8_t framesPerSecondFit() const noexcept {
        return framesPerSecondFit_.load();
    }

  private:
    /// A frame's times on this clock, from the host's capture to the screen.
    struct Times {
        std::uint32_t number{0};
        double capturedUs{0};
        double encodedUs{0};
        double sentUs{0};
        double receivedUs{0};
        double decodedUs{0};
        /// When the loop took it to show; 0 before.
        double takenUs{0};
    };
    /// One shown frame's stages, in milliseconds.
    struct Stages {
        double encode{0};
        double send{0};
        double network{0};
        double decode{0};
        /// Decoded to submitted, split at the loop taking it: `wait` before, `show` after.
        double submit{0};
        double wait{0};
        double show{0};
        double total{0};
    };

    struct Queued {
        wire::VideoPacket packet;
        std::uint64_t receivedUs{0};
        std::int64_t hostAheadUs{0};
    };
    void decodeLoop(const std::stop_token& stop);
    /// Warns once a stretch with no picture lasts `silenceWarned`. Decoding thread.
    void noteSilence();
    /// Judges how the guest keeps up (`GuestFit`). Decoding thread.
    void noteFit();
    /// One packet into the decoder, and a frame out if one is ready. Decoding thread.
    void decode(const Queued& queued);
    /// `picture`, of the frame numbered in it or else `number`, as the newest decoded.
    void finish(VideoCodec codec, Picture picture, std::uint32_t number);
    /// Whether a decoder holds frames that have not come out, and any it has ready.
    [[nodiscard]] bool behind() const;
    void collect();

    /// Each codec's decoder once its first packet came, and the codec of the frame shown last;
    /// on the decoding thread.
    std::map<VideoCodec, std::unique_ptr<VideoDecoder>> decoders_;
    /// Codecs whose decoder failed to open; their packets are dropped, not tried again.
    VideoCodecs undecodable_{0};
    /// The frame whose packets are going into the decoder, on the decoding thread.
    std::optional<Times> building_;
    /// The newest frame handed to its decoder; later packets of it are its parity.
    std::uint32_t handedOn_{0};
    /// Whole frames given to a decoder whose pictures have not come out, oldest first.
    std::deque<Times> inFlight_;
    /// The decoder last given a packet waits for a key frame.
    std::atomic<bool> keyWanted_{false};
    std::atomic<PictureMemory> memory_;
    /// When the last picture came out, or the stream began; packets since; on the decoding thread.
    std::chrono::steady_clock::time_point lastPictureAt_;
    std::size_t packetsSincePicture_{0};
    /// The current stretch without a picture was warned about.
    bool silent_{false};
    GuestFit fit_;
    std::atomic<bool> pyroWaveGivenUp_{false};
    std::atomic<std::uint8_t> framesPerSecondFit_{0};

    std::mutex queueMutex_;
    std::condition_variable_any queued_;
    std::vector<Queued> queue_;

    mutable std::mutex mutex_;
    /// Notified when `fresh_` gets a frame.
    std::condition_variable_any decodedOne_;
    FrameHold<Frame> fresh_;
    /// Hands the fresh frame over and notes when. Under `mutex_`.
    std::optional<Frame> handOver();
    std::optional<Frame> last_;
    /// Decoded frames not shown yet, oldest first.
    std::deque<Times> waiting_;
    std::deque<Stages> shown_;
    std::size_t decoded_{0};
    std::size_t dropped_{0};
    std::size_t late_{0};
    std::size_t presented_{0};
    std::size_t drawn_{0};
    /// The loop's presents into the window while streaming.
    std::size_t passes_{0};
    std::string codec_{"none"};
    std::size_t switches_{0};
    /// Last, so it stops before what it uses goes.
    std::jthread decoding_;
};

} // namespace opensu::stream

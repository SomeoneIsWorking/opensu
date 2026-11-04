// screen_capture — what the Gamescope this openSU runs in shows, frame by frame, from that
// Gamescope's PipeWire stream (the node its process owns, found through the peer of its Wayland
// socket): NV12 in shared memory, BT.709 full range, at the size Gamescope composes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace opensu::stream {

/// One frame as Gamescope composed it: the luma plane, then the interleaved chroma plane at half
/// the size each way, both rows `stride` bytes apart.
struct CapturedFrame {
    int width{0};
    int height{0};
    std::size_t stride{0};
    std::span<const std::uint8_t> luma;
    std::span<const std::uint8_t> chroma;
    /// When it arrived, on `wire::nowMicros`'s clock.
    std::uint64_t capturedUs{0};
};

class ScreenCapture {
  public:
    struct Setup {
        /// Gamescope's Wayland socket, whose peer owns the stream.
        std::filesystem::path gamescopeSocket;
        /// Each frame, on the capture's thread; the planes are Gamescope's only during the call.
        std::function<void(const CapturedFrame&)> frame;
    };

    /// Connects to the stream; throws `std::runtime_error` saying why it cannot.
    explicit ScreenCapture(Setup setup);
    ~ScreenCapture();
    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;
    ScreenCapture(ScreenCapture&&) = delete;
    ScreenCapture& operator=(ScreenCapture&&) = delete;

  private:
    class Stream;
    std::unique_ptr<Stream> stream_;
};

} // namespace opensu::stream

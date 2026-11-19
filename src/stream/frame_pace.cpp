#include "frame_pace.hpp"

namespace opensu::stream {
namespace {

constexpr std::uint64_t microsPerSecond = 1'000'000;

} // namespace

bool FramePace::due(std::uint64_t capturedUs, std::uint8_t framesPerSecond,
                    int capturedPerSecond) const noexcept {
    if (framesPerSecond == 0 || dueUs_ == 0 || capturedPerSecond <= 0) {
        return true;
    }
    // Half a capture early still meets it, so a capture a little early is not put off a whole one.
    const std::uint64_t halfCaptureUs =
        microsPerSecond / (std::uint64_t{2} * static_cast<std::uint64_t>(capturedPerSecond));
    return capturedUs + halfCaptureUs >= dueUs_;
}

void FramePace::sent(std::uint64_t capturedUs, std::uint8_t framesPerSecond) noexcept {
    if (framesPerSecond == 0) {
        dueUs_ = 0;
        return;
    }
    const std::uint64_t periodUs = microsPerSecond / framesPerSecond;
    // A schedule a whole frame behind (a new rate, a stall in capture) starts again from here.
    dueUs_ =
        dueUs_ == 0 || capturedUs >= dueUs_ + periodUs ? capturedUs + periodUs : dueUs_ + periodUs;
}

} // namespace opensu::stream

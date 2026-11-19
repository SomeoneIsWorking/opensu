// frame_hold — the decoded frames a guest holds for its loop, oldest first. Two are kept, so two
// landing in one vsync from a bunched arrival are both shown, one a vsync later; a hold that has
// lasted `trimAfter` takes in a row skips one, so it does not stay on as latency.
#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

namespace opensu::stream {

template <typename Frame> class FrameHold {
  public:
    static constexpr std::size_t kept = 2;
    static constexpr int trimAfter = 30;

    /// A frame was decoded; the oldest is let go, and returned, when more than `kept` wait.
    std::optional<Frame> put(Frame frame) {
        frames_.push_back(std::move(frame));
        if (frames_.size() <= kept) {
            return std::nullopt;
        }
        Frame let = std::move(frames_.front());
        frames_.pop_front();
        return let;
    }
    /// The oldest frame not yet taken.
    [[nodiscard]] std::optional<Frame> take() {
        if (frames_.empty()) {
            backlog_ = 0;
            return std::nullopt;
        }
        Frame frame = std::move(frames_.front());
        frames_.pop_front();
        if (frames_.empty()) {
            backlog_ = 0;
        } else if (++backlog_ >= trimAfter) {
            frames_.pop_front();
            backlog_ = 0;
        }
        return frame;
    }
    [[nodiscard]] bool empty() const noexcept {
        return frames_.empty();
    }

  private:
    std::deque<Frame> frames_;
    int backlog_{0};
};

} // namespace opensu::stream

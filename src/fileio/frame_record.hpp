// frame_record — frames written raw to a file or a named pipe at a fixed rate, paced by the clock,
// for a recording a tool encodes: a tick the loop draws no frame for repeats the last one, so the
// record plays at the speed it was drawn. A thread of its own writes, so the loop never waits on
// the reader.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace opensu::fileio {

class FrameRecord {
  public:
    using Clock = std::chrono::steady_clock;

    /// What a finished record wrote.
    struct Totals {
        /// Frames written in all, one per tick.
        std::uint64_t frames{0};
        /// Of those, the ticks that repeated the frame before for want of a new one.
        std::uint64_t repeated{0};
        /// Why writing stopped early, if it did.
        std::string error;
    };

    /// A frame's size in pixels.
    struct Size {
        int width{0};
        int height{0};
    };

    /// Writes RGBA frames of `size` to `path` at `fps`, from `start` on. Opening `path` happens on
    /// the writer, which waits there until a named pipe's reader opens it.
    FrameRecord(std::filesystem::path path, int fps, Size size, Clock::time_point start);
    ~FrameRecord();

    FrameRecord(const FrameRecord&) = delete;
    FrameRecord& operator=(const FrameRecord&) = delete;

    [[nodiscard]] int width() const noexcept {
        return width_;
    }
    [[nodiscard]] int height() const noexcept {
        return height_;
    }

    /// The ticks from the start up to `now`: one at the start, then one each 1/fps.
    [[nodiscard]] static std::uint64_t ticksBy(Clock::time_point start, int fps,
                                               Clock::time_point now);

    /// Whether a tick has come since the last frame was taken, so a frame is worth drawing.
    [[nodiscard]] bool due(Clock::time_point now) const;

    /// `rgba` for every tick come by `now`. A frame of another size repeats the last one instead.
    /// While the writer is behind, the ticks go to the frame already waiting.
    void add(std::vector<std::uint8_t> rgba, Clock::time_point now);

    /// The last frame repeated up to `now`, everything waiting written and the file closed.
    [[nodiscard]] Totals finish(Clock::time_point now);

  private:
    struct Waiting {
        std::vector<std::uint8_t> rgba;
        std::uint64_t ticks{0};
    };

    void write();
    void queue(std::vector<std::uint8_t> rgba, std::uint64_t ticks, bool fresh);

    std::filesystem::path path_;
    int fps_;
    int width_;
    int height_;
    Clock::time_point start_;
    /// Ticks handed to the writer so far.
    std::uint64_t taken_{0};

    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Waiting> waiting_;
    /// The newest frame, repeated when a tick has no other.
    std::vector<std::uint8_t> last_;
    bool finishing_{false};
    Totals totals_;
    std::thread writer_;
};

} // namespace opensu::fileio

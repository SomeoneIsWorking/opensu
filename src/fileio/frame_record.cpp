#include "fileio/frame_record.hpp"

#include <cstring>
#include <exception>
#include <utility>

#ifdef _WIN32
#include <fstream>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace opensu::fileio {

namespace {

// Frames waiting for the writer; past this, new ticks repeat the newest waiting frame.
constexpr std::size_t longestQueue = 4;

/// The file the writer appends to: a plain descriptor where a named pipe's reader may go away.
class Sink {
  public:
    explicit Sink(const std::filesystem::path& path, std::string& error) {
#ifdef _WIN32
        out_.open(path, std::ios::binary);
        if (!out_) {
            error = "cannot open " + path.string();
        }
#else
        // A reader that leaves makes a write fail with EPIPE, not end openSU with SIGPIPE.
        sigset_t pipe{};
        sigemptyset(&pipe);
        sigaddset(&pipe, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &pipe, nullptr);
        fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd_ < 0) {
            error = "cannot open " + path.string() + ": " + std::strerror(errno);
        }
#endif
    }
    ~Sink() {
#ifndef _WIN32
        if (fd_ >= 0) {
            ::close(fd_);
        }
#endif
    }
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;

    [[nodiscard]] bool write(const std::vector<std::uint8_t>& bytes, std::string& error) {
#ifdef _WIN32
        out_.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!out_) {
            error = "the write failed";
            return false;
        }
        return true;
#else
        std::size_t done = 0;
        while (done < bytes.size()) {
            const ssize_t wrote = ::write(fd_, bytes.data() + done, bytes.size() - done);
            if (wrote < 0) {
                if (errno == EINTR) {
                    continue;
                }
                error = std::string{"the write failed: "} + std::strerror(errno);
                return false;
            }
            done += static_cast<std::size_t>(wrote);
        }
        return true;
#endif
    }

  private:
#ifdef _WIN32
    std::ofstream out_;
#else
    int fd_{-1};
#endif
};

} // namespace

FrameRecord::FrameRecord(std::filesystem::path path, int fps, Size size, Clock::time_point start)
    : path_{std::move(path)}, fps_{fps}, width_{size.width}, height_{size.height}, start_{start},
      writer_{[this] {
          write();
      }} {
}

FrameRecord::~FrameRecord() {
    if (!writer_.joinable()) {
        return;
    }
    try {
        (void)finish(Clock::now());
    } catch (...) {
        // Out of memory or a broken mutex, with the writer still running: a thread left joinable
        // ends the program anyway, so it ends here, said plainly.
        std::terminate();
    }
}

std::uint64_t FrameRecord::ticksBy(Clock::time_point start, int fps, Clock::time_point now) {
    if (now < start || fps <= 0) {
        return 0;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
    return static_cast<std::uint64_t>(elapsed) * static_cast<std::uint64_t>(fps) / 1'000'000'000U +
           1;
}

bool FrameRecord::due(Clock::time_point now) const {
    return ticksBy(start_, fps_, now) > taken_;
}

void FrameRecord::add(std::vector<std::uint8_t> rgba, Clock::time_point now) {
    const std::uint64_t by = ticksBy(start_, fps_, now);
    if (by <= taken_) {
        return;
    }
    const std::uint64_t ticks = by - taken_;
    taken_ = by;
    const std::size_t size =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4U;
    const bool fresh = rgba.size() == size;
    queue(fresh ? std::move(rgba) : std::vector<std::uint8_t>{}, ticks, fresh);
}

void FrameRecord::queue(std::vector<std::uint8_t> rgba, std::uint64_t ticks, bool fresh) {
    {
        const std::lock_guard lock{mutex_};
        if (!fresh || waiting_.size() >= longestQueue) {
            if (!waiting_.empty()) {
                waiting_.back().ticks += ticks;
                return;
            }
            rgba.clear();
        }
        waiting_.push_back(Waiting{std::move(rgba), ticks});
    }
    changed_.notify_one();
}

FrameRecord::Totals FrameRecord::finish(Clock::time_point now) {
    if (!writer_.joinable()) {
        return totals_;
    }
    const std::uint64_t by = ticksBy(start_, fps_, now);
    if (by > taken_) {
        queue({}, by - taken_, false);
        taken_ = by;
    }
    {
        const std::lock_guard lock{mutex_};
        finishing_ = true;
    }
    changed_.notify_one();
    writer_.join();
    return totals_;
}

void FrameRecord::write() {
    std::string error;
    Sink sink{path_, error};
    bool writing = error.empty();
    for (;;) {
        Waiting next;
        {
            std::unique_lock lock{mutex_};
            changed_.wait(lock, [this] {
                return finishing_ || !waiting_.empty();
            });
            if (waiting_.empty()) {
                break;
            }
            next = std::move(waiting_.front());
            waiting_.pop_front();
        }
        const bool fresh = !next.rgba.empty();
        if (fresh) {
            last_ = std::move(next.rgba);
        }
        if (last_.empty()) {
            // Nothing drawn yet to repeat.
            continue;
        }
        for (std::uint64_t tick = 0; writing && tick < next.ticks; ++tick) {
            writing = sink.write(last_, error);
        }
        const std::lock_guard lock{mutex_};
        totals_.frames += next.ticks;
        totals_.repeated += fresh ? next.ticks - 1 : next.ticks;
    }
    const std::lock_guard lock{mutex_};
    totals_.error = error;
}

} // namespace opensu::fileio

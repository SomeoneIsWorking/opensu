#include "command.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <thread>

#include <array>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "lucent/log.h"

namespace opensu::launch {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr auto step = std::chrono::milliseconds{5};

bool isExecutable(const fs::path& candidate) {
    std::error_code ec;
    return fs::is_regular_file(candidate, ec) && access(candidate.c_str(), X_OK) == 0;
}

int exitCode(int status) {
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

constexpr auto readSlice = std::chrono::milliseconds{100};
/// How long a stopped child has to end on SIGTERM before it is killed.
constexpr auto termGrace = std::chrono::seconds{2};

/// Closes a descriptor when it goes out of scope.
class Descriptor {
  public:
    explicit Descriptor(int fd) noexcept : fd_{fd} {
    }
    ~Descriptor() {
        close();
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    [[nodiscard]] int get() const noexcept {
        return fd_;
    }
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

  private:
    int fd_;
};

/// Ends the child's process group: SIGTERM, then SIGKILL after the grace, and reaps it.
void endGroup(pid_t pid) {
    ::kill(-pid, SIGTERM);
    const Clock::time_point until = Clock::now() + termGrace;
    int status = 0;
    while (Clock::now() < until) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            ::kill(-pid, SIGKILL);
            return;
        }
        std::this_thread::sleep_for(step);
    }
    ::kill(-pid, SIGKILL);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
}

} // namespace

fs::path resolveExecutable(const std::string& program, const std::vector<fs::path>& path) {
    if (program.empty()) {
        return {};
    }
    if (program.find('/') != std::string::npos) {
        return isExecutable(program) ? fs::path{program} : fs::path{};
    }
    const auto found = std::ranges::find_if(path, [&](const fs::path& dir) {
        return isExecutable(dir / program);
    });
    return found != path.end() ? *found / program : fs::path{};
}

std::optional<int> runCommand(const std::string& program, const std::vector<std::string>& args,
                              std::chrono::milliseconds timeout) {
    Argv argv{program, args};

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = -1;
    const int spawned =
        posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0) {
        lucent::warn("launch", "cannot run {}: {}", program, std::strerror(spawned));
        return std::nullopt;
    }

    const Clock::time_point until = Clock::now() + timeout;
    int status = 0;
    for (;;) {
        const pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            return exitCode(status);
        }
        if (done < 0 && errno != EINTR) {
            return std::nullopt;
        }
        if (Clock::now() >= until) {
            lucent::error("launch", "{} did not finish in time; killing it", program);
            ::kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            return std::nullopt;
        }
        std::this_thread::sleep_for(step);
    }
}

namespace {

/// Writes all of `data` to `fd`; false when the reader is gone or the write failed. A closed
/// reader raises SIGPIPE, which is held back for the write and taken so it never reaches the
/// process.
bool writeAll(int fd, std::string_view data) {
    sigset_t pipeSignal;
    sigemptyset(&pipeSignal);
    sigaddset(&pipeSignal, SIGPIPE);
    sigset_t previous;
    pthread_sigmask(SIG_BLOCK, &pipeSignal, &previous);
    bool ok = true;
    while (ok && !data.empty()) {
        const ssize_t put = write(fd, data.data(), data.size());
        if (put < 0 && errno == EINTR) {
            continue;
        }
        ok = put > 0;
        if (ok) {
            data.remove_prefix(static_cast<std::size_t>(put));
        }
    }
    if (!ok) {
        const timespec none{0, 0};
        sigtimedwait(&pipeSignal, nullptr, &none);
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    return ok;
}

std::optional<int> stream(const std::string& program, const std::vector<std::string>& args,
                          const std::function<void(std::string_view)>& onLine,
                          const std::stop_token& stop, CaptureErrors errors,
                          const std::optional<std::string_view>& stdinLine = std::nullopt) {
    std::array<int, 2> ends{};
    if (pipe2(ends.data(), O_CLOEXEC) != 0) {
        lucent::warn("launch", "cannot make a pipe for {}: {}", program, std::strerror(errno));
        return std::nullopt;
    }
    Descriptor reader{ends[0]};
    Descriptor writer{ends[1]};
    std::array<int, 2> inEnds{-1, -1};
    if (stdinLine && pipe2(inEnds.data(), O_CLOEXEC) != 0) {
        lucent::warn("launch", "cannot make a pipe for {}: {}", program, std::strerror(errno));
        return std::nullopt;
    }
    Descriptor stdinReader{inEnds[0]};
    Descriptor stdinWriter{inEnds[1]};

    Argv argv{program, args};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (stdinLine) {
        posix_spawn_file_actions_adddup2(&actions, stdinReader.get(), STDIN_FILENO);
    } else {
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    }
    posix_spawn_file_actions_adddup2(&actions, writer.get(), STDOUT_FILENO);
    if (errors == CaptureErrors::Merged) {
        posix_spawn_file_actions_adddup2(&actions, writer.get(), STDERR_FILENO);
    } else {
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    }
    // A group of its own, so stopping it reaches the processes the child starts.
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    pid_t pid = -1;
    const int spawned =
        posix_spawnp(&pid, program.c_str(), &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    if (spawned != 0) {
        lucent::warn("launch", "cannot run {}: {}", program, std::strerror(spawned));
        return std::nullopt;
    }
    writer.close();
    stdinReader.close();
    if (stdinLine) {
        // A child that has already gone is found out by its status below.
        static_cast<void>(writeAll(stdinWriter.get(), *stdinLine) &&
                          writeAll(stdinWriter.get(), "\n"));
        stdinWriter.close();
    }

    std::string pending;
    std::array<char, 4096> chunk{};
    bool open = true;
    while (open) {
        if (stop.stop_requested()) {
            endGroup(pid);
            return std::nullopt;
        }
        pollfd watch{.fd = reader.get(), .events = POLLIN, .revents = 0};
        const int ready = poll(&watch, 1, static_cast<int>(readSlice.count()));
        if (ready < 0 && errno != EINTR) {
            break;
        }
        if (ready <= 0) {
            continue;
        }
        const ssize_t got = read(reader.get(), chunk.data(), chunk.size());
        if (got <= 0) {
            open = got < 0 && errno == EINTR;
            continue;
        }
        pending.append(chunk.data(), static_cast<std::size_t>(got));
        for (std::size_t end = pending.find('\n'); end != std::string::npos;
             end = pending.find('\n')) {
            onLine(std::string_view{pending}.substr(0, end));
            pending.erase(0, end + 1);
        }
    }
    if (!pending.empty()) {
        onLine(pending);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return std::nullopt;
        }
    }
    return exitCode(status);
}

} // namespace

std::optional<int> runStreaming(const std::string& program, const std::vector<std::string>& args,
                                const std::function<void(std::string_view)>& onLine,
                                const std::stop_token& stop) {
    return stream(program, args, onLine, stop, CaptureErrors::Merged);
}

std::optional<Captured> runCaptured(const std::string& program,
                                    const std::vector<std::string>& args, CaptureErrors errors) {
    Captured captured;
    const std::optional<int> status = stream(
        program, args,
        [&captured](std::string_view line) {
            captured.output.append(line);
            captured.output.push_back('\n');
        },
        std::stop_token{}, errors);
    if (!status) {
        return std::nullopt;
    }
    captured.status = *status;
    return captured;
}

std::optional<Captured> runCapturedWithLine(const std::string& program,
                                            const std::vector<std::string>& args,
                                            std::string_view line) {
    Captured captured;
    const std::optional<int> status = stream(
        program, args,
        [&captured](std::string_view out) {
            captured.output.append(out);
            captured.output.push_back('\n');
        },
        std::stop_token{}, CaptureErrors::Merged, line);
    if (!status) {
        return std::nullopt;
    }
    captured.status = *status;
    return captured;
}

} // namespace opensu::launch

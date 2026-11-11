#include "launch_group.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "child_environment.hpp"
#include "command.hpp"
#include "descriptor.hpp"
#include "lucent/log.h"

namespace opensu::launch {
namespace {

using Clock = std::chrono::steady_clock;

/// How long the tree has to end on SIGTERM before it is killed, as the systemd scope's
/// TimeoutStopSec.
constexpr long termWaitMs = 5000;
/// How often the keeper sweeps the tree with SIGKILL until it is empty.
constexpr int killSweepMs = 50;
/// How long start() waits for the game's exec.
constexpr int execWaitMs = 5000;
/// How long stop() waits for the keeper beyond its own SIGKILL sweep.
constexpr auto keeperWait = std::chrono::seconds{10};
constexpr auto step = std::chrono::milliseconds{5};
/// The processes one /proc sweep reads, and how many of them can be the keeper's.
constexpr std::size_t sweepCapacity = 8192;
constexpr std::size_t treeCapacity = 1024;
/// The one byte the keeper writes when the game itself has exited.
constexpr char gameEnded = 'e';

/// What the keeper and the game need, built before the fork.
struct Plan {
    char** argv;
    char** envp;
    const char* program;
    /// Null for openSU's own.
    const char* workingDirectory;
    /// -1 for openSU's own stdout and stderr.
    int output;
    /// The keeper's end of the pipe whose reader is openSU: the game's end goes down it, and
    /// POLLERR on it says openSU is gone.
    int events;
    /// The writer of the pipe start() reads the errno of a failed start from; it closes on exec.
    int exec;
};

// Everything from here to the class runs between fork and exec or _exit, in a child of a
// threaded process: syscalls only, no allocation, no locks, no logging.

[[noreturn]] void fail(int execFd, int error) {
    static_cast<void>(::write(execFd, &error, sizeof error));
    ::_exit(127);
}

long nowMs() {
    timespec now{};
    ::clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}

/// Closes descriptors `first` to `last`; one by one up to the descriptor limit on a kernel
/// before close_range (5.9).
void closeRange(unsigned int first, unsigned int last) {
    if (::syscall(SYS_close_range, first, last, 0) == 0 || errno != ENOSYS) {
        return;
    }
    rlimit open{};
    const rlim_t limit = ::getrlimit(RLIMIT_NOFILE, &open) == 0 ? open.rlim_cur : 1024;
    for (rlim_t fd = first; fd <= last && fd < limit; ++fd) {
        ::close(static_cast<int>(fd));
    }
}

/// Closes every descriptor but stdio and the plan's, so the keeper holds none of openSU's.
void keepOnly(const Plan& plan) {
    std::array<int, 3> kept{plan.output, plan.events, plan.exec};
    std::sort(kept.begin(), kept.end());
    unsigned int from = 3;
    for (const int fd : kept) {
        if (fd < static_cast<int>(from)) {
            continue;
        }
        if (fd > static_cast<int>(from)) {
            closeRange(from, static_cast<unsigned int>(fd) - 1);
        }
        from = static_cast<unsigned int>(fd) + 1;
    }
    closeRange(from, ~0U);
}

/// Writes `value`'s decimal digits at `at`; returns the end.
char* putNumber(char* at, long value) {
    std::array<char, 24> digits{};
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0);
    while (count > 0) {
        *at++ = digits[--count];
    }
    return at;
}

/// `pid`'s parent from `/proc/<pid>/stat`, or -1.
pid_t parentOf(long pid) {
    std::array<char, 40> path{};
    char* at = path.data();
    for (const char c : std::string_view{"/proc/"}) {
        *at++ = c;
    }
    at = putNumber(at, pid);
    for (const char c : std::string_view{"/stat"}) {
        *at++ = c;
    }
    const int fd = ::open(path.data(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    std::array<char, 512> stat{};
    const ssize_t got = ::read(fd, stat.data(), stat.size() - 1);
    ::close(fd);
    if (got <= 0) {
        return -1;
    }
    // "pid (name) S ppid ...": the name can hold either parenthesis, so the last ')' ends it.
    const char* close = nullptr;
    for (ssize_t i = 0; i < got; ++i) {
        if (stat[static_cast<std::size_t>(i)] == ')') {
            close = &stat[static_cast<std::size_t>(i)];
        }
    }
    if (close == nullptr || close + 4 >= stat.data() + got) {
        return -1;
    }
    long parent = 0;
    for (const char* digit = close + 4; *digit >= '0' && *digit <= '9'; ++digit) {
        parent = (parent * 10) + (*digit - '0');
    }
    return static_cast<pid_t>(parent);
}

struct Process {
    pid_t pid;
    pid_t parent;
};

/// Sends `signal` to every descendant of the keeper, and to the game's process group while the
/// game is unreaped and so still holds its id.
void signalTree(pid_t game, bool gameReaped, int signal) {
    if (!gameReaped) {
        static_cast<void>(::kill(-game, signal));
    }
    std::array<Process, sweepCapacity> table{};
    std::size_t count = 0;
    const int proc = ::open("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (proc >= 0) {
        alignas(8) std::array<char, 8192> entries{};
        long got = 0;
        while (count < table.size() &&
               (got = ::syscall(SYS_getdents64, proc, entries.data(), entries.size())) > 0) {
            for (long offset = 0; offset < got && count < table.size();) {
                // linux_dirent64: d_ino (8), d_off (8), d_reclen (2), d_type (1), d_name.
                unsigned short length = 0;
                std::memcpy(&length, entries.data() + offset + 16, sizeof length);
                const char* name = entries.data() + offset + 19;
                long pid = 0;
                bool numeric = *name != '\0';
                for (const char* c = name; *c != '\0'; ++c) {
                    numeric = numeric && *c >= '0' && *c <= '9';
                    pid = (pid * 10) + (*c - '0');
                }
                if (numeric) {
                    table[count++] =
                        Process{.pid = static_cast<pid_t>(pid), .parent = parentOf(pid)};
                }
                offset += length;
            }
        }
        ::close(proc);
    }
    std::array<pid_t, treeCapacity> tree{};
    std::size_t members = 1;
    tree[0] = ::getpid();
    for (bool grew = true; grew;) {
        grew = false;
        for (std::size_t i = 0; i < count && members < tree.size(); ++i) {
            bool known = false;
            bool child = false;
            for (std::size_t m = 0; m < members; ++m) {
                known = known || tree[m] == table[i].pid;
                child = child || tree[m] == table[i].parent;
            }
            if (child && !known) {
                tree[members++] = table[i].pid;
                grew = true;
            }
        }
    }
    for (std::size_t m = 1; m < members; ++m) {
        static_cast<void>(::kill(tree[m], signal));
    }
}

/// The game: its own process group, every signal at its default and none blocked.
[[noreturn]] void runGame(const Plan& plan) {
    struct sigaction initial{};
    initial.sa_handler = SIG_DFL;
    for (int signal = 1; signal < NSIG; ++signal) {
        if (signal != SIGKILL && signal != SIGSTOP) {
            static_cast<void>(::sigaction(signal, &initial, nullptr));
        }
    }
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, nullptr);
    if (::setpgid(0, 0) != 0) {
        fail(plan.exec, errno);
    }
    if (plan.output >= 0 &&
        (::dup2(plan.output, STDOUT_FILENO) < 0 || ::dup2(plan.output, STDERR_FILENO) < 0)) {
        fail(plan.exec, errno);
    }
    if (plan.workingDirectory != nullptr && ::chdir(plan.workingDirectory) != 0) {
        fail(plan.exec, errno);
    }
    ::execvpe(plan.program, plan.argv, plan.envp);
    fail(plan.exec, errno);
}

/// Ends with the game's own status: its exit code, or its signal raised on the keeper.
[[noreturn]] void exitAs(int gameStatus) {
    if (!WIFSIGNALED(gameStatus)) {
        ::_exit(WEXITSTATUS(gameStatus));
    }
    const int signal = WTERMSIG(gameStatus);
    // The game's crash is the game's; the keeper leaves no core of its own.
    const rlimit noCore{0, 0};
    static_cast<void>(::setrlimit(RLIMIT_CORE, &noCore));
    static_cast<void>(::prctl(PR_SET_DUMPABLE, 0, 0, 0, 0));
    struct sigaction initial{};
    initial.sa_handler = SIG_DFL;
    static_cast<void>(::sigaction(signal, &initial, nullptr));
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, nullptr);
    static_cast<void>(::kill(::getpid(), signal));
    ::_exit(128 + signal);
}

/// The keeper: the child subreaper of the game's tree, so whatever leaves the game's group or
/// session still reparents here. Stops the tree on SIGTERM, SIGINT or SIGHUP, or when openSU is
/// gone; exits once the tree is empty.
[[noreturn]] void runKeeper(const Plan& plan) {
    sigset_t held;
    sigemptyset(&held);
    for (const int signal : {SIGCHLD, SIGTERM, SIGINT, SIGHUP}) {
        sigaddset(&held, signal);
    }
    sigprocmask(SIG_SETMASK, &held, nullptr);
    struct sigaction ignored{};
    ignored.sa_handler = SIG_IGN;
    static_cast<void>(::sigaction(SIGPIPE, &ignored, nullptr));
    keepOnly(plan);
    if (::prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
        fail(plan.exec, errno);
    }
    const int signals = ::signalfd(-1, &held, SFD_CLOEXEC);
    if (signals < 0) {
        fail(plan.exec, errno);
    }
    const pid_t game = ::fork();
    if (game < 0) {
        fail(plan.exec, errno);
    }
    if (game == 0) {
        runGame(plan);
    }
    ::close(plan.exec);
    if (plan.output >= 0) {
        ::close(plan.output);
    }

    int gameStatus = 0;
    bool gameReaped = false;
    bool stopping = false;
    long killAt = 0;
    std::array<pollfd, 2> watched{pollfd{.fd = signals, .events = POLLIN, .revents = 0},
                                  pollfd{.fd = plan.events, .events = 0, .revents = 0}};
    const auto beginStop = [&] {
        if (!stopping) {
            stopping = true;
            killAt = nowMs() + termWaitMs;
            signalTree(game, gameReaped, SIGTERM);
        }
    };
    for (;;) {
        int status = 0;
        const pid_t done = ::waitpid(-1, &status, WNOHANG);
        if (done > 0) {
            if (done == game) {
                gameStatus = status;
                gameReaped = true;
                static_cast<void>(::write(plan.events, &gameEnded, 1));
            }
            continue;
        }
        if (done < 0 && errno == ECHILD) {
            exitAs(gameStatus);
        }
        int timeout = -1;
        if (stopping) {
            const long left = killAt - nowMs();
            if (left <= 0) {
                signalTree(game, gameReaped, SIGKILL);
                timeout = killSweepMs;
            } else {
                timeout = static_cast<int>(left);
            }
        }
        if (::poll(watched.data(), watched.size(), timeout) < 0 && errno != EINTR) {
            beginStop();
        }
        if ((watched[1].revents & (POLLERR | POLLHUP)) != 0) {
            // openSU's end is closed only when openSU is gone.
            watched[1].fd = -1;
            beginStop();
        }
        if ((watched[0].revents & POLLIN) != 0) {
            signalfd_siginfo taken{};
            if (::read(signals, &taken, sizeof taken) == sizeof taken &&
                taken.ssi_signo != SIGCHLD) {
                beginStop();
            }
        }
    }
}

/// A launch under a keeper of its own (`runKeeper`), for a Linux without a reachable systemd
/// user manager.
class Keeper final : public LaunchGroup {
  public:
    bool start(const std::string& group, const std::string& program,
               const std::vector<std::string>& args, std::string& failure,
               const std::vector<std::string>& environment,
               const std::filesystem::path& workingDirectory, const std::filesystem::path& output,
               int& exitStatus) override;
    bool running(int& exitStatus) override;
    bool startedRunning(int& exitStatus) override;
    void stop(int& exitStatus) override;

  private:
    /// Reaps the keeper once it has exited; true while it runs.
    bool keeperRunning(int& exitStatus);
    /// Waits up to `keeperWait` for the keeper, SIGKILLs it after that, and reaps it.
    void reapKeeper(int& exitStatus);

    pid_t keeper_{-1};
    /// openSU's end of the keeper's pipe; closing it is how a dying openSU tells the keeper.
    std::optional<Descriptor> events_;
    bool gameEnded_{false};
};

bool Keeper::start(const std::string& group, const std::string& program,
                   const std::vector<std::string>& args, std::string& failure,
                   const std::vector<std::string>& environment,
                   const std::filesystem::path& workingDirectory,
                   const std::filesystem::path& output, int& exitStatus) {
    if (keeper_ > 0) {
        failure = "an instance is already running";
        return false;
    }
    Argv argv{program, args};
    ChildEnvironment childEnvironment{environment};
    const Descriptor outputFile{
        output.empty() ? -1
                       : ::open(output.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644)};
    if (!output.empty() && outputFile.get() < 0) {
        failure = "cannot write " + output.string() + ": " + std::strerror(errno);
        return false;
    }
    // Both pipes close on exec, so no other program openSU starts holds either end.
    std::array<int, 2> events{};
    std::array<int, 2> exec{};
    if (::pipe2(events.data(), O_CLOEXEC) != 0) {
        failure = "could not start " + programName(program) + ": " + std::strerror(errno);
        return false;
    }
    Descriptor eventsReader{events[0]};
    Descriptor eventsWriter{events[1]};
    if (::pipe2(exec.data(), O_CLOEXEC) != 0) {
        failure = "could not start " + programName(program) + ": " + std::strerror(errno);
        return false;
    }
    Descriptor execReader{exec[0]};
    Descriptor execWriter{exec[1]};
    const std::string directory = workingDirectory.string();
    const Plan plan{.argv = argv.data(),
                    .envp = childEnvironment.data(),
                    .program = argv.program().c_str(),
                    .workingDirectory = directory.empty() ? nullptr : directory.c_str(),
                    .output = outputFile.get(),
                    .events = eventsWriter.get(),
                    .exec = execWriter.get()};

    const pid_t pid = ::fork();
    if (pid < 0) {
        failure = "could not start " + programName(program) + ": " + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        runKeeper(plan);
    }
    eventsWriter.close();
    execWriter.close();
    keeper_ = pid;
    exitStatus = -1;
    gameEnded_ = false;

    // The exec pipe ends empty once the game has exec'd, or holds the errno of what failed.
    int error = 0;
    std::size_t got = 0;
    pollfd watch{.fd = execReader.get(), .events = POLLIN, .revents = 0};
    for (;;) {
        const int ready = ::poll(&watch, 1, execWaitMs);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            error = ready == 0 ? ETIMEDOUT : errno;
            ::kill(keeper_, SIGTERM);
            break;
        }
        const ssize_t read =
            ::read(execReader.get(), reinterpret_cast<char*>(&error) + got, sizeof error - got);
        if (read < 0 && errno == EINTR) {
            continue;
        }
        if (read <= 0) {
            break;
        }
        got += static_cast<std::size_t>(read);
        if (got == sizeof error) {
            break;
        }
    }
    if (error != 0) {
        reapKeeper(exitStatus);
        failure = "could not start " + programName(program) + ": " + std::strerror(error);
        return false;
    }
    ::fcntl(eventsReader.get(), F_SETFL, ::fcntl(eventsReader.get(), F_GETFL) | O_NONBLOCK);
    events_.emplace(eventsReader.release());
    lucent::info("launch", "{} started under keeper {} for {}", program, pid, group);
    return true;
}

bool Keeper::running(int& exitStatus) {
    return keeperRunning(exitStatus);
}

bool Keeper::startedRunning(int& exitStatus) {
    if (!keeperRunning(exitStatus)) {
        return false;
    }
    char event = 0;
    while (events_ && ::read(events_->get(), &event, 1) == 1) {
        gameEnded_ = gameEnded_ || event == gameEnded;
    }
    return !gameEnded_;
}

void Keeper::stop(int& exitStatus) {
    if (keeper_ <= 0) {
        return;
    }
    lucent::info("launch", "stopping keeper {}", keeper_);
    ::kill(keeper_, SIGTERM);
    reapKeeper(exitStatus);
}

bool Keeper::keeperRunning(int& exitStatus) {
    if (keeper_ <= 0) {
        return false;
    }
    int status = 0;
    const pid_t done = ::waitpid(keeper_, &status, WNOHANG);
    if (done == 0) {
        return true;
    }
    if (done == keeper_) {
        exitStatus = exitStatusOf(status);
    }
    keeper_ = -1;
    events_.reset();
    return false;
}

void Keeper::reapKeeper(int& exitStatus) {
    const Clock::time_point until = Clock::now() + keeperWait;
    int status = 0;
    pid_t done = 0;
    while ((done = ::waitpid(keeper_, &status, WNOHANG)) == 0) {
        if (Clock::now() >= until) {
            lucent::error("launch", "keeper {} outlived its tree's SIGKILL; killing it", keeper_);
            ::kill(keeper_, SIGKILL);
            while ((done = ::waitpid(keeper_, &status, 0)) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(step);
    }
    if (done == keeper_) {
        exitStatus = exitStatusOf(status);
    }
    keeper_ = -1;
    events_.reset();
}

} // namespace

std::unique_ptr<LaunchGroup> makeKeeper() {
    return std::make_unique<Keeper>();
}

} // namespace opensu::launch

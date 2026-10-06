#include "command.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.hpp"
#include "lucent/log.h"

namespace iideck::launch {
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

} // namespace iideck::launch

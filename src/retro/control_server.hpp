// control_server — opensu-retro's loopback control channel for the shell: the control file that
// announces its port (or the load failure), the routes, and the hand-over that runs each command
// on the frame loop between frames, never beside retro_run.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "lucent/http.h"

namespace opensu::retro {

enum class ControlCommand : std::uint8_t { SaveState, LoadState, Reset };

/// What a command came to: an HTTP status and, unless it is 200, the reason.
struct ControlAnswer {
    int status{200};
    std::string reason;
    /// Whether the state slot holds a state once the command ran.
    bool hasState{};
};

class ControlServer {
  public:
    /// How long a request waits for the frame loop to take it before it is answered 503.
    static constexpr std::chrono::seconds loopWait{3};

    /// `file` is where the port is announced; `core` and `rom` are what /status names.
    ControlServer(std::filesystem::path file, std::string core, std::string rom);
    /// Answers any waiting request 503, stops listening and removes the control file.
    ~ControlServer();
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;
    ControlServer(ControlServer&&) = delete;
    ControlServer& operator=(ControlServer&&) = delete;

    /// Listens on a free loopback port and writes `{"port": N, "pid": P}` to the control file.
    /// False, logged, when either fails; the game then runs without a channel.
    [[nodiscard]] bool start();

    /// What /status reports from now on. Frame loop only.
    void publish(std::uint64_t frame, bool hasState);

    /// Runs every command waiting, on the calling thread. Frame loop only, between frames.
    void serve(const std::function<ControlAnswer(ControlCommand)>& perform);

    /// Leaves `{"error": reason}` in `file` for the shell, which removes it; logged when it cannot.
    static void reportLoadFailure(const std::filesystem::path& file, const std::string& reason);

    /// Routes a request. Any thread.
    [[nodiscard]] lucent::http::Response handle(const lucent::http::Request& request);

  private:
    struct Pending {
        ControlCommand command{};
        bool taken{false};
        bool answered{false};
        ControlAnswer answer;
    };

    [[nodiscard]] lucent::http::Response run(ControlCommand command);

    std::filesystem::path file_;
    std::string core_;
    std::string rom_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<std::shared_ptr<Pending>> queue_;
    bool closing_{false};
    std::uint64_t frame_{0};
    bool hasState_{false};
    bool announced_{false};
    /// Last, so its handler threads stop before the members they use go.
    lucent::http::Server server_;
};

} // namespace opensu::retro

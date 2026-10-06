// launch — one transient systemd user scope that owns everything a launch starts.
#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <sys/types.h>

namespace iideck::launch {

/// Runs a command inside its own systemd scope. The scope's cgroup is the
/// ownership: processes that setsid or double-fork stay in it, so stopping the
/// scope ends all of them. Every call is thread-safe, so a controller handler
/// can kill an instance another thread is waiting on.
class Instance {
  public:
    Instance() = default;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    ~Instance();

    /// Starts `program` in the scope `unit`. Returns once the scope exists, so a
    /// following stop() or kill() always reaches it.
    [[nodiscard]] bool start(const std::string& unit, const std::string& program,
                             const std::vector<std::string>& args, std::string& failure);

    /// True while anything is left in the scope. The started process may exit
    /// while processes it spawned carry on, which still counts as running.
    [[nodiscard]] bool running();

    /// Asks every process in the scope to terminate, then reaps the started one.
    void stop();

    /// SIGKILLs every process in the scope, then reaps the started one.
    void kill();

    /// The current scope's unit name, empty when nothing was started.
    [[nodiscard]] std::string unit() const;

    /// True when `program` names an executable file, directly or in one of `path`.
    [[nodiscard]] static bool findExecutable(const std::string& program,
                                             const std::vector<std::filesystem::path>& path);

  private:
    void endScope(const std::vector<std::string>& systemctlArgs);
    void reapChild();

    mutable std::mutex mutex_;
    std::string unit_;
    pid_t child_{-1};
};

} // namespace iideck::launch

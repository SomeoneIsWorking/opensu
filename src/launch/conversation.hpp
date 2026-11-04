// conversation — a child spoken with line by line over its stdin and stdout, as polkit's agent
// helper is: started in a process group of its own with stderr discarded, its lines read as they
// come without blocking. Dropping it closes its stdin and stdout, as polkit's own agents end their
// helper; a child still running 200 ms later has its group sent SIGTERM, then SIGKILL, each result
// logged, and one no signal reaches is left to end on its closed pipes. POSIX only
// (`posix_child.cpp`).
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::launch {

class Conversation {
  public:
    /// `program` started with `args`; null when it could not be.
    [[nodiscard]] static std::unique_ptr<Conversation> start(const std::string& program,
                                                             const std::vector<std::string>& args);

    /// The child's two pipe ends this side holds.
    struct Ends {
        int input;
        int output;
    };

    Conversation(int pid, Ends ends) noexcept;
    ~Conversation();
    Conversation(const Conversation&) = delete;
    Conversation& operator=(const Conversation&) = delete;
    Conversation(Conversation&&) = delete;
    Conversation& operator=(Conversation&&) = delete;

    /// The descriptor that turns readable when the child writes or ends, for `poll`.
    [[nodiscard]] int readable() const noexcept {
        return output_;
    }
    /// Writes `line` and a newline; false once the child no longer reads.
    bool say(std::string_view line);
    /// The whole lines the child has written since the last call, reading only what is there.
    [[nodiscard]] std::vector<std::string> heard();
    /// Whether the child has closed its output.
    [[nodiscard]] bool ended() const noexcept {
        return ended_;
    }

  private:
    int pid_;
    int input_;
    int output_;
    bool ended_{false};
    std::string pending_;
};

} // namespace opensu::launch

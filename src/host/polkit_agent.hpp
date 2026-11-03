// polkit_agent — openSU's own polkit authentication agent, for what openSU itself starts through
// pkexec. It registers on the system bus for openSU's process (a unix-process subject, as
// `pkttyagent --process` does), so polkit asks it instead of the desktop's agent when a child of
// openSU wants an action, and the desktop's agent stays the session's for everything else. polkit's
// questions become a `Prompt` the shell draws; the answer goes to polkit-agent-helper-1. Linux
// only, over sd-bus on a thread of its own.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "polkit_authentication.hpp"

namespace opensu::host {

class PolkitAgent {
  public:
    virtual ~PolkitAgent() = default;

    /// The question polkit waits on, if one is.
    [[nodiscard]] virtual std::optional<polkit::Prompt> prompt() const = 0;
    /// Answers question `serial` with `secret`, which it takes, leaving it empty. Swapped, never
    /// moved, so no copy is left in the caller's buffer.
    virtual void answer(std::uint64_t serial, std::string& secret) = 0;
    /// Ends the authentication being asked for; polkit tells pkexec it was dismissed.
    virtual void cancel() = 0;
};

/// The agent for this process, its messages in `locale`; null, with the reason logged, when there
/// is no system bus, no polkit or no helper.
[[nodiscard]] std::unique_ptr<PolkitAgent> startPolkitAgent(std::string locale);

} // namespace opensu::host

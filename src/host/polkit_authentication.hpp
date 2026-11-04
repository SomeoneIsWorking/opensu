// polkit_authentication — one authentication polkit hands openSU's agent: the helper run for the
// user with the cookie, what PAM's modules say and ask turned into what the player is shown
// (waiting while a module works without them, asking while one wants an answer), the answers
// written back and wiped, a refused password asked again with a note, and the outcome. Driven by
// the agent's thread; nothing here knows the bus.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "launch/conversation.hpp"

namespace opensu::host::polkit {

/// What the player is shown while an authentication is under way.
struct Prompt {
    enum class State : std::uint8_t {
        /// A module works without the player (a finger, a face, a phone) or checks an answer.
        Waiting,
        /// PAM asks `question` (PAM_PROMPT_ECHO_OFF or ECHO_ON).
        Asking,
    };
    /// New for each authentication polkit begins.
    std::uint64_t authentication{};
    State state{State::Waiting};
    /// New for each question, so an answer goes to the question it was typed for.
    std::uint64_t serial{};
    /// polkit's message for the action.
    std::string message;
    /// Whose authentication it is.
    std::string user;
    /// PAM's question ("Password: "), while asking.
    std::string question;
    /// Whether the answer is shown as typed (PAM_PROMPT_ECHO_ON).
    bool shown{false};
    /// What PAM said last (PAM_TEXT_INFO, PAM_ERROR_MSG) or why it asks again; empty for none.
    std::string note;
};

/// The note a refused password is asked again with, when PAM says nothing itself.
inline constexpr const char* refusedNote = "That password was not accepted. Try again.";

class Authentication {
  public:
    enum class Outcome : std::uint8_t {
        Pending,
        Authenticated,
        /// The helper could not run, or ended before asking anything.
        Failed,
    };

    /// For `user`, with the helper at `helper`, numbering itself and its questions from
    /// `serials`, which outlives it; nothing starts before `begin`.
    Authentication(std::filesystem::path helper, std::string message, std::string user,
                   std::string cookie, std::uint64_t& serials);

    /// Starts the helper and hands it the cookie.
    void begin();
    /// The helper's output, for `poll`; -1 while none runs.
    [[nodiscard]] int readable() const noexcept;
    /// Takes what the helper has written.
    void pump();
    /// Writes `secret` as the answer to question `serial`, then wipes it; a stale serial is
    /// dropped.
    void answer(std::uint64_t serial, std::string& secret);

    /// What the player is shown while the authentication is under way; nothing once it has an
    /// outcome.
    [[nodiscard]] std::optional<Prompt> prompt() const;
    [[nodiscard]] Outcome outcome() const noexcept {
        return outcome_;
    }
    /// Why it failed; empty otherwise.
    [[nodiscard]] const std::string& failure() const noexcept {
        return failure_;
    }

  private:
    void heard(const std::string& line);
    void fail(std::string why);

    std::filesystem::path helper_;
    std::string user_;
    std::string cookie_;
    std::uint64_t* serials_;
    Prompt prompt_;
    bool answered_{false};
    Outcome outcome_{Outcome::Pending};
    std::string failure_;
    std::unique_ptr<launch::Conversation> helperRun_;
};

} // namespace opensu::host::polkit

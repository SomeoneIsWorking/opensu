// polkit_helper — polkit-agent-helper-1's side of one authentication: where distributions install
// it, and what each line it writes means (a PAM prompt, an error, a note, success or failure),
// its text unescaped as `g_strescape` wrote it. The agent writes the cookie, then each answer.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace opensu::host::polkit {

/// Where the setuid helper is installed: Fedora, Arch and Debian; older releases' libexec.
inline constexpr std::array<std::string_view, 3> helperPaths{
    "/usr/lib/polkit-1/polkit-agent-helper-1", "/usr/libexec/polkit-agent-helper-1",
    "/usr/lib/policykit-1/polkit-agent-helper-1"};

struct HelperLine {
    enum class Kind : std::uint8_t {
        /// PAM asks for a secret (PAM_PROMPT_ECHO_OFF) or for a shown answer (PAM_PROMPT_ECHO_ON).
        AskHidden,
        AskShown,
        /// PAM_ERROR_MSG and PAM_TEXT_INFO.
        Error,
        Info,
        /// The helper's verdict, its last line.
        Success,
        Failure,
        Unknown,
    };
    Kind kind{Kind::Unknown};
    std::string text;
};

/// What `line` (one line of the helper's stdout, without its newline) says.
[[nodiscard]] HelperLine parseHelperLine(std::string_view line);

/// `text` with `g_strescape`'s escapes (\n, \t, \\, \", \ooo and the rest) undone.
[[nodiscard]] std::string unescape(std::string_view text);

} // namespace opensu::host::polkit

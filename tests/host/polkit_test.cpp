// polkit's agent side without the bus: the helper's lines read as PAM prompts, notes and verdicts,
// and one authentication over a stand-in helper that speaks the helper's protocol (the cookie on
// stdin, then notes, prompts and a verdict): waiting with PAM's note while a module works without
// the player, the note changed in place, success with no question asked, a question after a
// module gave up, a refused password asked again with a note under a new serial, the right one
// authenticated, a helper that cannot run or refuses before asking failed.
#include "host/polkit_authentication.hpp"
#include "host/polkit_helper.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>

#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include "launch/procfs.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using host::polkit::Authentication;
using host::polkit::HelperLine;
using test::expect;
using Kind = HelperLine::Kind;
using State = host::polkit::Prompt::State;

fs::path helperScript(const fs::path& root, const std::string& name, const std::string& body) {
    fs::create_directories(root);
    const fs::path file = root / name;
    {
        std::ofstream out{file};
        out << "#!/bin/sh\n" << body << "\n";
    }
    chmod(file.c_str(), S_IRWXU);
    return file;
}

/// Pumps `authentication` until `done` holds, for at most ten seconds.
bool pumpUntil(Authentication& authentication, const std::function<bool()>& done) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        pollfd watch{.fd = authentication.readable(), .events = POLLIN, .revents = 0};
        constexpr int sliceMs = 100;
        static_cast<void>(poll(&watch, watch.fd >= 0 ? 1 : 0, sliceMs));
        authentication.pump();
    }
    return true;
}

void testHelperLines() {
    const HelperLine ask = host::polkit::parseHelperLine("PAM_PROMPT_ECHO_OFF Password: ");
    expect(ask.kind == Kind::AskHidden && ask.text == "Password: ", "a hidden prompt");
    expect(host::polkit::parseHelperLine("PAM_PROMPT_ECHO_ON Name:").kind == Kind::AskShown,
           "a shown prompt");
    const HelperLine info =
        host::polkit::parseHelperLine(R"(PAM_TEXT_INFO Place your finger\non the \"reader\")");
    expect(info.kind == Kind::Info && info.text == "Place your finger\non the \"reader\"",
           "a note is unescaped");
    expect(host::polkit::parseHelperLine("PAM_ERROR_MSG Locked").kind == Kind::Error, "an error");
    expect(host::polkit::parseHelperLine("SUCCESS").kind == Kind::Success, "success");
    expect(host::polkit::parseHelperLine("FAILURE").kind == Kind::Failure, "failure");
    expect(host::polkit::parseHelperLine("PAM_PROMPT_ECHO_OFFX").kind == Kind::Unknown,
           "a word that only starts like a prompt is unknown");
    expect(host::polkit::unescape(R"(caf\303\251\t\\)") == "caf\xc3\xa9\t\\",
           "octal bytes, tabs and backslashes are undone");
}

/// The state `authentication` shows now; nothing once it has an outcome.
std::optional<State> stateOf(const Authentication& authentication) {
    const std::optional<host::polkit::Prompt> now = authentication.prompt();
    return now ? std::optional<State>{now->state} : std::nullopt;
}

/// The note `authentication` shows now; empty once it has an outcome.
std::string noteOf(const Authentication& authentication) {
    const std::optional<host::polkit::Prompt> now = authentication.prompt();
    return now ? now->note : std::string{};
}

void testRefusedThenAccepted(const fs::path& root) {
    const fs::path log = root / "helper.log";
    const fs::path helper = helperScript(root, "helper",
                                         "read cookie\necho \"$1 $cookie\" >> '" + log.string() +
                                             "'\nprintf 'PAM_PROMPT_ECHO_OFF Password: \\n'\n"
                                             "read answer\n"
                                             "if [ \"$answer\" = right ]; then echo SUCCESS; "
                                             "else echo FAILURE; fi");
    std::uint64_t serials = 0;
    Authentication authentication{helper, "Install session mode", "player", "cookie-1", serials};
    authentication.begin();
    expect(stateOf(authentication) == State::Waiting,
           "the authentication waits before the helper asks");
    expect(pumpUntil(authentication,
                     [&] {
                         return stateOf(authentication) == State::Asking;
                     }),
           "the helper's question is asked");
    const host::polkit::Prompt first = test::need(authentication.prompt(), "a first question");
    expect(first.question == "Password: " && first.user == "player" &&
               first.message == "Install session mode" && !first.shown && first.note.empty(),
           "the question names the user and the action, masked");
    std::string wrong = "wrong";
    authentication.answer(first.serial, wrong);
    expect(wrong.empty(), "the answer is wiped once written");
    expect(stateOf(authentication) == State::Waiting,
           "the authentication waits while the answer is checked");
    expect(pumpUntil(authentication,
                     [&] {
                         return stateOf(authentication) == State::Asking;
                     }),
           "a refused password is asked again");
    const host::polkit::Prompt again = test::need(authentication.prompt(), "a second question");
    expect(again.serial > first.serial && again.authentication == first.authentication &&
               again.note == host::polkit::refusedNote,
           "under a new serial of the same authentication, saying the password was not accepted");
    std::string stale = "right";
    authentication.answer(first.serial, stale);
    expect(stateOf(authentication) == State::Asking && stale.empty(),
           "an answer to the old question is dropped, and wiped");
    std::string right = "right";
    authentication.answer(again.serial, right);
    expect(pumpUntil(authentication,
                     [&] {
                         return authentication.outcome() != Authentication::Outcome::Pending;
                     }) &&
               authentication.outcome() == Authentication::Outcome::Authenticated,
           "the right password authenticates");
    std::ifstream calls{log};
    std::string firstCall;
    std::string secondCall;
    std::getline(calls, firstCall);
    std::getline(calls, secondCall);
    expect(firstCall == "player cookie-1" && secondCall == "player cookie-1",
           "each try runs the helper for the user with the cookie on stdin");
}

void testWaitingModule(const fs::path& root) {
    const fs::path confirmed = root / "confirmed";
    const fs::path done = root / "done";
    const fs::path helper =
        helperScript(root, "phone",
                     "read cookie\nprintf 'PAM_TEXT_INFO Confirm on the phone\\n'\n"
                     "until [ -e '" +
                         confirmed.string() +
                         "' ]; do sleep 0.05; done\n"
                         "printf 'PAM_TEXT_INFO Confirmed\\n'\n"
                         "until [ -e '" +
                         done.string() +
                         "' ]; do sleep 0.05; done\n"
                         "echo SUCCESS");
    std::uint64_t serials = 0;
    Authentication phone{helper, "Install session mode", "player", "c", serials};
    phone.begin();
    expect(pumpUntil(phone,
                     [&] {
                         return noteOf(phone) == "Confirm on the phone";
                     }),
           "a module working without the player is shown with its note");
    const host::polkit::Prompt waiting = test::need(phone.prompt(), "the waiting authentication");
    expect(waiting.state == State::Waiting && waiting.message == "Install session mode",
           "waiting, for the action, with nothing asked");
    std::ofstream{confirmed}.put('\n');
    expect(pumpUntil(phone,
                     [&] {
                         return noteOf(phone) == "Confirmed";
                     }) &&
               stateOf(phone) == State::Waiting,
           "a later note replaces it in place");
    std::ofstream{done}.put('\n');
    expect(pumpUntil(phone,
                     [&] {
                         return phone.outcome() != Authentication::Outcome::Pending;
                     }) &&
               phone.outcome() == Authentication::Outcome::Authenticated && !phone.prompt(),
           "success with no question asked authenticates and shows nothing more");

    const fs::path gaveUp =
        helperScript(root, "gave-up",
                     "read cookie\nprintf 'PAM_TEXT_INFO Confirm on the phone\\n'\n"
                     "printf 'PAM_ERROR_MSG Not confirmed\\n'\n"
                     "printf 'PAM_PROMPT_ECHO_ON Code: \\n'\nread code\n"
                     "if [ \"$code\" = 42 ]; then echo SUCCESS; else echo FAILURE; fi");
    Authentication fallback{gaveUp, "x", "player", "c", serials};
    fallback.begin();
    expect(pumpUntil(fallback,
                     [&] {
                         return stateOf(fallback) == State::Asking;
                     }),
           "a module that gives up leaves PAM to ask");
    const host::polkit::Prompt code = test::need(fallback.prompt(), "the code question");
    expect(code.shown && code.question == "Code: " && code.note == "Not confirmed" &&
               code.authentication != waiting.authentication,
           "shown as typed, with the module's last word, in an authentication of its own");
    std::string answer = "42";
    fallback.answer(code.serial, answer);
    expect(pumpUntil(fallback,
                     [&] {
                         return fallback.outcome() != Authentication::Outcome::Pending;
                     }) &&
               fallback.outcome() == Authentication::Outcome::Authenticated,
           "the answer authenticates");
}

void testHelperFailures(const fs::path& root) {
    std::uint64_t serials = 0;
    Authentication missing{root / "no-helper", "x", "player", "c", serials};
    missing.begin();
    expect(missing.outcome() == Authentication::Outcome::Failed && !missing.failure().empty(),
           "a helper that cannot run fails");
    const fs::path refusing = helperScript(
        root, "refusing", "read cookie\nprintf 'PAM_ERROR_MSG Account locked\\n'\necho FAILURE");
    Authentication refused{refusing, "x", "player", "c", serials};
    refused.begin();
    expect(pumpUntil(refused,
                     [&] {
                         return refused.outcome() != Authentication::Outcome::Pending;
                     }) &&
               refused.outcome() == Authentication::Outcome::Failed &&
               refused.failure() == "Account locked",
           "a helper that refuses before asking fails with PAM's reason");
}

void testStartTime() {
    expect(launch::procfs::startTime(getpid()).value_or(0) > 0,
           "this process's start time is read");
}

} // namespace

int main() try {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "polkit";
    fs::remove_all(root);
    testHelperLines();
    testRefusedThenAccepted(root);
    testWaitingModule(root);
    testHelperFailures(root);
    testStartTime();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

#include "polkit_authentication.hpp"

#include <utility>

#include "lucent/log.h"
#include "polkit_helper.hpp"
#include "secrets/secret.hpp"

namespace opensu::host::polkit {

Authentication::Authentication(std::filesystem::path helper, std::string message, std::string user,
                               std::string cookie, std::uint64_t& serials)
    : helper_{std::move(helper)}, user_{std::move(user)}, cookie_{std::move(cookie)},
      serials_{&serials} {
    prompt_.authentication = ++*serials_;
    prompt_.message = std::move(message);
    prompt_.user = user_;
}

void Authentication::begin() {
    prompt_.state = Prompt::State::Waiting;
    answered_ = false;
    helperRun_ = launch::Conversation::start(helper_.string(), {user_});
    if (!helperRun_ || !helperRun_->say(cookie_)) {
        fail("polkit's agent helper " + helper_.string() + " could not be run");
    }
}

int Authentication::readable() const noexcept {
    return helperRun_ ? helperRun_->readable() : -1;
}

void Authentication::pump() {
    if (!helperRun_ || outcome_ != Outcome::Pending) {
        return;
    }
    for (const std::string& line : helperRun_->heard()) {
        heard(line);
        if (!helperRun_) {
            return;
        }
    }
    if (helperRun_->ended() && outcome_ == Outcome::Pending) {
        fail("polkit's agent helper ended without a verdict");
    }
}

void Authentication::heard(const std::string& line) {
    using Kind = HelperLine::Kind;
    const HelperLine said = parseHelperLine(line);
    switch (said.kind) {
    case Kind::AskHidden:
    case Kind::AskShown:
        prompt_.question = said.text;
        prompt_.shown = said.kind == Kind::AskShown;
        prompt_.serial = ++*serials_;
        prompt_.state = Prompt::State::Asking;
        return;
    case Kind::Error:
    case Kind::Info:
        lucent::info("polkit", "PAM says: {}", said.text);
        prompt_.note = said.text;
        return;
    case Kind::Success:
        lucent::info("polkit", "{} authenticated", user_);
        outcome_ = Outcome::Authenticated;
        helperRun_.reset();
        return;
    case Kind::Failure:
        if (!answered_) {
            fail(prompt_.note.empty() ? std::string{"polkit's agent helper refused"}
                                      : prompt_.note);
            return;
        }
        lucent::info("polkit", "the password for {} was not accepted; asking again", user_);
        if (prompt_.note.empty()) {
            prompt_.note = refusedNote;
        }
        helperRun_.reset();
        begin();
        return;
    case Kind::Unknown:
        lucent::warn("polkit", "the agent helper said something unknown: {}", said.text);
        return;
    }
}

void Authentication::answer(std::uint64_t serial, std::string& secret) {
    if (prompt_.state != Prompt::State::Asking || serial != prompt_.serial || !helperRun_) {
        secrets::wipe(secret);
        return;
    }
    prompt_.state = Prompt::State::Waiting;
    answered_ = true;
    prompt_.note.clear();
    const bool written = helperRun_->say(secret);
    secrets::wipe(secret);
    if (!written) {
        fail("polkit's agent helper stopped reading");
    }
}

std::optional<Prompt> Authentication::prompt() const {
    if (outcome_ != Outcome::Pending) {
        return std::nullopt;
    }
    return prompt_;
}

void Authentication::fail(std::string why) {
    lucent::warn("polkit", "authenticating {} failed: {}", user_, why);
    outcome_ = Outcome::Failed;
    failure_ = std::move(why);
    helperRun_.reset();
}

} // namespace opensu::host::polkit

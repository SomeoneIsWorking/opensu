#include "login_copies.hpp"

#include "lucent/log.h"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

/// What every ask of this kind is about.
constexpr std::string_view steam = "steam";

} // namespace

bool LoginCopies::ask(const Peer& peer, Clock::time_point now, std::string& refusal) {
    if (!exported_) {
        refusal = "this openSU has no fumarole to sign in to Steam with";
        return false;
    }
    std::string why;
    std::optional<std::string> login = exported_(why);
    if (!login) {
        refusal = why.empty() ? "this openSU is not signed in to Steam" : why;
        return false;
    }
    secrets::wipe(*login);
    if (!asks_.ask(peer, std::string{steam}, {}, now, refusal)) {
        return false;
    }
    lucent::info("lan", "{} asks for this openSU's Steam sign-in", peer.name);
    return true;
}

std::optional<wire::LoginVerdict> LoginCopies::status(std::string_view peerId,
                                                      Clock::time_point now) {
    const std::optional<PeerAsk<std::monostate>::Status> asked = asks_.status(peerId, steam, now);
    if (!asked) {
        return std::nullopt;
    }
    wire::LoginVerdict verdict{
        .verdict = std::string{key(asked->verdict)}, .login = {}, .refusal = {}};
    if (asked->verdict != Verdict::Accepted) {
        return verdict;
    }
    std::string why;
    std::optional<std::string> login = exported_(why);
    if (!login) {
        lucent::warn("lan", "the Steam sign-in allowed cannot be read: {}", why);
        verdict.verdict = std::string{key(Verdict::Declined)};
        verdict.refusal = why.empty() ? "its Steam sign-in cannot be read" : why;
        return verdict;
    }
    verdict.login = std::move(*login);
    lucent::info("lan", "sending the Steam sign-in to the openSU allowed it");
    return verdict;
}

std::optional<Peer> LoginCopies::question(Clock::time_point now) const {
    std::optional<PeerAsk<std::monostate>::Question> asked = asks_.question(now);
    if (!asked) {
        return std::nullopt;
    }
    return std::move(asked->peer);
}

bool LoginCopies::decide(std::string_view peerId, bool allow, Clock::time_point now) {
    return asks_.decide(peerId, steam, now, [allow](const Peer& peer, std::monostate) {
        lucent::info("lan", "{} {} this openSU's Steam sign-in", peer.name,
                     allow ? "may take" : "was refused");
        return allow ? Verdict::Accepted : Verdict::Declined;
    });
}

} // namespace opensu::lan

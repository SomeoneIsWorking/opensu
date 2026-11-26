#include "profile_copies.hpp"

#include <utility>

#include "lucent/log.h"

namespace opensu::lan {

bool ProfileCopies::ask(const Peer& peer, std::string_view profileId, Clock::time_point now,
                        std::string& refusal) {
    std::optional<profiles::Profile> profile = profiles_.find(profileId);
    if (!profile) {
        refusal = "no profile here has that id";
        return false;
    }
    const std::string name = profile->name;
    if (!asks_.ask(peer, std::string{profileId}, std::move(*profile), now, refusal)) {
        return false;
    }
    lucent::info("lan", "{} asks for a copy of the profile {}", peer.name, name);
    return true;
}

std::optional<ProfileCopies::Status>
ProfileCopies::status(std::string_view peerId, std::string_view profileId, Clock::time_point now) {
    std::optional<PeerAsk<profiles::Profile>::Status> asked = asks_.status(peerId, profileId, now);
    if (!asked) {
        return std::nullopt;
    }
    Status status{.verdict = asked->verdict, .profile = std::move(asked->subject)};
    status.profile.links.clear();
    return status;
}

std::optional<ProfileCopies::Question> ProfileCopies::question(Clock::time_point now) const {
    std::optional<PeerAsk<profiles::Profile>::Question> asked = asks_.question(now);
    if (!asked) {
        return std::nullopt;
    }
    return Question{.peer = std::move(asked->peer), .profile = std::move(asked->subject)};
}

bool ProfileCopies::decide(std::string_view peerId, std::string_view profileId, bool allow,
                           Clock::time_point now, std::string& error) {
    bool kept = true;
    const bool asked = asks_.decide(
        peerId, profileId, now, [&](const Peer& peer, const profiles::Profile& profile) {
            if (allow) {
                kept = profiles_.link(profileId, peerId, true, error);
            }
            const bool given = allow && kept;
            lucent::info("lan", "{} {} a copy of the profile {}", peer.name,
                         given ? "may take" : "was refused", profile.name);
            return given ? Verdict::Accepted : Verdict::Declined;
        });
    if (!asked) {
        error = "the ask lapsed; ask again from the other openSU";
        return false;
    }
    return kept;
}

} // namespace opensu::lan

#include "profile_copies.hpp"

#include "lucent/log.h"

namespace opensu::lan {

void ProfileCopies::lapse(Clock::time_point now) {
    if (pending_ && now - pending_->started > patience) {
        pending_.reset();
    }
}

bool ProfileCopies::ask(const Peer& peer, std::string_view profileId, Clock::time_point now,
                        std::string& refusal) {
    const std::optional<profiles::Profile> profile = profiles_.find(profileId);
    if (!profile) {
        refusal = "no profile here has that id";
        return false;
    }
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (pending_ && pending_->peer.id == peer.id && pending_->profile.id == profileId) {
        return true;
    }
    if (pending_ && pending_->verdict == Verdict::Waiting) {
        refusal = "another openSU's ask waits for the player";
        return false;
    }
    pending_ = Pending{.peer = peer, .profile = *profile, .started = now, .verdict = {}};
    lucent::info("lan", "{} asks for a copy of the profile {}", peer.name, profile->name);
    return true;
}

std::optional<ProfileCopies::Status>
ProfileCopies::status(std::string_view peerId, std::string_view profileId, Clock::time_point now) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->peer.id != peerId || pending_->profile.id != profileId) {
        return std::nullopt;
    }
    Status status{.verdict = pending_->verdict, .profile = pending_->profile};
    status.profile.links.clear();
    if (pending_->verdict != Verdict::Waiting) {
        pending_.reset();
    }
    return status;
}

std::optional<ProfileCopies::Question> ProfileCopies::question(Clock::time_point now) const {
    const std::lock_guard lock{mutex_};
    if (!pending_ || pending_->verdict != Verdict::Waiting || now - pending_->started > patience) {
        return std::nullopt;
    }
    return Question{.peer = pending_->peer, .profile = pending_->profile};
}

bool ProfileCopies::decide(std::string_view peerId, std::string_view profileId, bool allow,
                           Clock::time_point now, std::string& error) {
    const std::lock_guard lock{mutex_};
    lapse(now);
    if (!pending_ || pending_->peer.id != peerId || pending_->profile.id != profileId) {
        error = "the ask lapsed; ask again from the other openSU";
        return false;
    }
    bool kept = true;
    if (allow) {
        kept = profiles_.link(profileId, peerId, true, error);
    }
    pending_->verdict = allow && kept ? Verdict::Accepted : Verdict::Declined;
    lucent::info("lan", "{} {} a copy of the profile {}", pending_->peer.name,
                 pending_->verdict == Verdict::Accepted ? "may take" : "was refused",
                 pending_->profile.name);
    return kept;
}

} // namespace opensu::lan

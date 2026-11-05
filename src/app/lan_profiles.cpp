#include "lan_profiles.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "lucent/log.h"

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view pagePrefix = "lanprofiles:";
constexpr std::string_view copyPrefix = "lanprofiles.copy:";
/// How often the asker looks at how its ask stands, and how long it waits for the player there.
constexpr std::chrono::seconds pollEvery{1};
constexpr std::chrono::seconds waitAtMost{130};

/// `text` split at its first `:`.
std::pair<std::string, std::string> splitFirst(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return {std::string{text}, {}};
    }
    return {std::string{text.substr(0, colon)}, std::string{text.substr(colon + 1)}};
}

} // namespace

std::string LanProfiles::pageOf(const std::string& peerId) {
    return std::string{pagePrefix} + peerId;
}

bool LanProfiles::owns(const std::string& id) {
    return id.starts_with(pagePrefix) || id.starts_with(copyPrefix);
}

void LanProfiles::opening(const std::string& id) {
    if (id.starts_with(pagePrefix)) {
        ask(id.substr(pagePrefix.size()));
    }
}

void LanProfiles::ask(const std::string& peerId) {
    Listing& listing = listings_[peerId];
    if (listing.asking) {
        return;
    }
    const bool asked = lan_.askPeers(
        [arrived = arrived_, peerId](const lan::LanNode& node, const std::stop_token& stop) {
            Listing answer;
            std::string error;
            std::optional<std::vector<lan::wire::ListedProfile>> got =
                node.profilesOf(stop, peerId, error);
            if (stop.stop_requested()) {
                return;
            }
            if (got) {
                answer.profiles = std::move(*got);
            } else {
                answer.error = error.empty() ? std::string{"it did not answer"} : std::move(error);
            }
            const std::lock_guard lock{arrived->mutex};
            arrived->listings.emplace_back(peerId, std::move(answer));
        });
    listing.asking = asked;
    if (!asked) {
        listing.error =
            lan_.node() == nullptr ? "LAN is off" : "busy with another openSU; try again";
    }
}

std::optional<ui::SettingsCategory> LanProfiles::subPage(const std::string& id) const {
    if (!id.starts_with(pagePrefix)) {
        return std::nullopt;
    }
    const std::string peerId = id.substr(pagePrefix.size());
    const std::string host = lan_.peerName(peerId);
    ui::SettingsCategory page{id, "Profiles on " + host, {}};
    const auto found = listings_.find(peerId);
    if (found == listings_.end() || (found->second.asking && found->second.profiles.empty())) {
        page.rows.push_back(ui::makeRow("lanprofiles.asking", RowKind::Info, "Asking " + host,
                                        "Its profiles show here", ""));
        return page;
    }
    const Listing& listing = found->second;
    if (!listing.error.empty()) {
        page.rows.push_back(ui::makeRow("lanprofiles.error", RowKind::Info,
                                        "Cannot list its profiles", listing.error, ""));
        return page;
    }
    for (const lan::wire::ListedProfile& profile : listing.profiles) {
        const std::string row = std::string{copyPrefix} + peerId + ":" + profile.id;
        if (profile.linked) {
            page.rows.push_back(ui::makeRow(row, RowKind::Info, profile.name,
                                            "Linked: its saves stay the same here and on " + host,
                                            "Linked"));
        } else if (waitingPeer_ == peerId && waitingProfile_ == profile.id) {
            page.rows.push_back(ui::makeRow(row, RowKind::Info, profile.name,
                                            "Allow the copy on " + host, "Waiting"));
        } else {
            page.rows.push_back(ui::makeRow(
                row, RowKind::Action, "Copy " + profile.name,
                "Asks " + host + " first; the copy's saves then stay the same on both", ""));
        }
    }
    if (listing.profiles.empty()) {
        page.rows.push_back(ui::makeRow("lanprofiles.none", RowKind::Info, "No profiles", "", ""));
    }
    return page;
}

void LanProfiles::run(const std::string& id) {
    if (!id.starts_with(copyPrefix)) {
        return;
    }
    const auto [peerId, profileId] = splitFirst(std::string_view{id}.substr(copyPrefix.size()));
    const auto found = listings_.find(peerId);
    if (found == listings_.end() || !waitingPeer_.empty()) {
        return;
    }
    const auto profile =
        std::ranges::find(found->second.profiles, profileId, &lan::wire::ListedProfile::id);
    if (profile == found->second.profiles.end() || profile->linked) {
        return;
    }
    copy(peerId, *profile);
}

void LanProfiles::copy(const std::string& peerId, const lan::wire::ListedProfile& profile) {
    const bool asked = lan_.askPeers([arrived = arrived_, peerId, profile](
                                         const lan::LanNode& node, const std::stop_token& stop) {
        Arrived::Copy outcome{.peerId = peerId, .profile = std::nullopt, .refusal = {}};
        std::string error;
        if (!node.askCopy(stop, peerId, profile.id, error)) {
            outcome.refusal = error;
        } else {
            const auto until = std::chrono::steady_clock::now() + waitAtMost;
            outcome.refusal = "no answer came";
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < until) {
                std::this_thread::sleep_for(pollEvery);
                const std::optional<lan::wire::CopyVerdict> verdict =
                    node.copyStatus(stop, peerId, profile.id, error);
                if (!verdict) {
                    outcome.refusal = error;
                    break;
                }
                if (verdict->verdict == "accepted") {
                    outcome.profile = verdict->profile;
                    outcome.refusal.clear();
                    break;
                }
                if (verdict->verdict == "declined") {
                    outcome.refusal = "it was declined";
                    break;
                }
            }
        }
        if (stop.stop_requested()) {
            return;
        }
        const std::lock_guard lock{arrived->mutex};
        arrived->copies.push_back(std::move(outcome));
    });
    if (!asked) {
        hooks_.say("Cannot ask for " + profile.name + " now; try again", true);
        return;
    }
    waitingPeer_ = peerId;
    waitingProfile_ = profile.id;
    lucent::info("lan", "asking {} for a copy of the profile {}", lan_.peerName(peerId),
                 profile.id);
    hooks_.say("Allow the copy of " + profile.name + " on " + lan_.peerName(peerId), false);
    hooks_.refresh();
}

void LanProfiles::keepCopy(const Arrived::Copy& copy) {
    const std::string host = lan_.peerName(copy.peerId);
    if (!copy.profile) {
        lucent::info("lan", "no copy of a profile from {}: {}", host, copy.refusal);
        hooks_.say("No copy from " + host + ": " + copy.refusal, true);
        return;
    }
    const profiles::Profile kept{.id = copy.profile->id,
                                 .name = copy.profile->name,
                                 .picture = copy.profile->picture,
                                 .links = {}};
    std::string error;
    if (!profiles_.adopt(kept, copy.peerId, error)) {
        hooks_.say("Cannot keep " + kept.name + ": " + error, true);
        return;
    }
    lucent::info("lan", "copied the profile {} from {}, linked with it", kept.id, host);
    saves_.syncAll(kept.id);
    hooks_.say("Copied " + kept.name + " from " + host, false);
}

void LanProfiles::unlink(const std::string& profileId, const std::string& peerId) {
    const std::string host = lan_.peerName(peerId);
    std::string error;
    if (lan_.node() == nullptr) {
        if (!profiles_.link(profileId, peerId, false, error)) {
            hooks_.say("Cannot unlink: " + error, true);
            return;
        }
        hooks_.say("Unlinked here; " + host + " is not told while LAN is off", false);
        hooks_.refresh();
        return;
    }
    const bool asked = lan_.askPeers([arrived = arrived_, peerId, profileId,
                                      host](const lan::LanNode& node, const std::stop_token& stop) {
        std::string why;
        const bool done = node.unlink(stop, peerId, profileId, why);
        const std::lock_guard lock{arrived->mutex};
        arrived->said.emplace_back(done ? "Unlinked from " + host : "Cannot unlink: " + why, !done);
    });
    if (!asked) {
        hooks_.say("Cannot unlink now; try again", true);
    }
}

void LanProfiles::service() {
    serviceQuestion();
    std::vector<std::pair<std::string, Listing>> listings;
    std::vector<Arrived::Copy> copies;
    std::vector<std::pair<std::string, bool>> said;
    {
        const std::lock_guard lock{arrived_->mutex};
        listings.swap(arrived_->listings);
        copies.swap(arrived_->copies);
        said.swap(arrived_->said);
    }
    for (auto& [peerId, listing] : listings) {
        listings_[peerId] = std::move(listing);
    }
    for (const Arrived::Copy& copy : copies) {
        waitingPeer_.clear();
        waitingProfile_.clear();
        keepCopy(copy);
        ask(copy.peerId);
    }
    for (const auto& [text, refusal] : said) {
        hooks_.say(text, refusal);
    }
    if (!listings.empty() || !copies.empty() || !said.empty()) {
        hooks_.refresh();
    }
}

void LanProfiles::serviceQuestion() {
    const lan::LanNode* node = lan_.node();
    std::optional<lan::ProfileCopies::Question> question =
        node != nullptr ? node->copyQuestion() : std::nullopt;
    if (question == asked_) {
        return;
    }
    closeQuestion();
    asked_ = std::move(question);
    if (!asked_) {
        return;
    }
    lucent::info("lan", "{} asks for a copy of the profile {}", asked_->peer.name,
                 asked_->profile.id);
    std::vector<gpu2d::ui::Dialog::Choice> choices;
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Decline",
                                                .chosen =
                                                    [this] {
                                                        answerQuestion(false);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Normal,
                                                .exits = true});
    choices.push_back(gpu2d::ui::Dialog::Choice{.label = "Allow",
                                                .chosen =
                                                    [this] {
                                                        answerQuestion(true);
                                                    },
                                                .kind = gpu2d::ui::ButtonKind::Primary,
                                                .exits = false});
    const std::string& peer = asked_->peer.name;
    const std::string& name = asked_->profile.name;
    questionTitle_ = "Copy " + name + " to " + peer + "?";
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<gpu2d::ui::Dialog>(
        questionTitle_,
        peer + " asks for a copy of " + name + ". Allowed, " + name + "'s saves stay the same " +
            "here and on " + peer + " until either unlinks it.",
        std::move(choices), 0));
}

void LanProfiles::answerQuestion(bool allow) {
    questionTitle_.clear();
    lan::LanNode* node = lan_.node();
    if (node == nullptr || !asked_) {
        return;
    }
    const lan::ProfileCopies::Question asked = *asked_;
    std::string error;
    if (!node->answerCopy(asked.peer.id, asked.profile.id, allow, error)) {
        hooks_.say("Cannot copy " + asked.profile.name + ": " + error, true);
        return;
    }
    hooks_.say(allow ? asked.profile.name + " is linked with " + asked.peer.name
                     : "Declined " + asked.peer.name,
               false);
    hooks_.refresh();
}

void LanProfiles::closeQuestion() {
    if (questionTitle_.empty()) {
        return;
    }
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == "dialog" && screen->title() == questionTitle_) {
            navigator_.closeFrom(*screen);
            break;
        }
    }
    questionTitle_.clear();
}

} // namespace opensu::app

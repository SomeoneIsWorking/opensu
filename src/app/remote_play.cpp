#include "remote_play.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "launch/process_tree.hpp"
#include "lucent/log.h"
#include "secrets/secret.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::app {
namespace {

using Clock = std::chrono::steady_clock;

/// How long an answer waits for the guest to ask how its ask stands.
constexpr std::chrono::seconds answerKept{30};
/// How often a guest waiting on the game's start hears its step again: a datagram can be lost.
constexpr std::chrono::milliseconds stageEvery{500};
/// How often guests hear the game's panel and shape, which a core may name late.
constexpr std::chrono::seconds factsEvery{1};
/// The longest side of a picture sent at its own size, for the guest to upscale.
constexpr int lowResSide = 640;

void wipe(lan::StreamKeys& keys) {
    secrets::wipeBytes(keys.host.data(), keys.host.size());
    secrets::wipeBytes(keys.guest.data(), keys.guest.size());
}

/// `names` as "A", "A and B", "A, B and C".
std::string listOf(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            text += i + 1 == names.size() ? " and " : ", ";
        }
        text += names[i];
    }
    return text;
}

} // namespace

RemotePlay::RemotePlay(Screen screen, Hooks hooks)
    : screen_{std::move(screen)}, hooks_{std::move(hooks)} {
}

RemotePlay::~RemotePlay() {
    if (guest_) {
        guest_->end("openSU closed");
    }
    const std::lock_guard lock{hostMutex_};
    if (hosted_) {
        for (const std::unique_ptr<Guest>& guest : hosted_->guests) {
            if (guest->stream) {
                guest->stream->end("openSU closed");
            }
        }
    }
    for (Ask& ask : asks_) {
        wipe(ask.keys);
    }
}

std::uint16_t RemotePlay::openStream(Hosted& hosted, Guest& guest, const lan::StreamKeys& keys) {
    stream::HostInput& player = *guest.input;
    guest.stream = std::make_unique<stream::HostStream>(
        stream::HostStream::Setup{.keys = keys,
                                  .guest = guest.peer.name,
                                  .pad =
                                      [&player](const stream::wire::PadState& state) {
                                          player.pad(state);
                                      },
                                  .keyboard =
                                      [&player](const stream::wire::KeyState& state) {
                                          player.keys(state);
                                      },
                                  .mouse =
                                      [&player](const stream::wire::MouseState& state) {
                                          player.mouse(state);
                                      },
                                  .mapping =
                                      [&player](const stream::wire::PadMapping& mapping) {
                                          player.mapping(mapping);
                                      },
                                  .encoders = stream::encodableHere()});
    hosted.audience.add(*guest.stream);
    return guest.stream->port();
}

std::uint16_t RemotePlay::admit(Hosted& hosted, const Seat& seat, const lan::StreamKeys& keys) {
    auto guest = std::make_unique<Guest>();
    static_cast<Seat&>(*guest) = seat;
    guest->input = std::make_unique<stream::HostInput>(
        seat.withKeys ? std::optional{screen_.x11Display} : std::nullopt,
        seat.own ? hooks_.lendPad() : hooks_.seatPad());
    const std::uint16_t port = openStream(hosted, *guest, keys);
    lucent::info("remote", "{} is guest {} of {}, its pad {}, its stream on UDP port {}",
                 seat.peer.name, hosted.guests.size() + 1,
                 hosted.title.empty() ? hosted.gameId : hosted.title, guest->input->padNode(),
                 port);
    hosted.guests.push_back(std::move(guest));
    return port;
}

std::uint16_t RemotePlay::takeOver(Hosted& hosted, Guest& guest, const lan::Peer& peer,
                                   const lan::StreamKeys& keys) {
    if (guest.stream) {
        hosted.audience.remove(*guest.stream);
        guest.stream->end("Continued on " + peer.name);
        guest.stream.reset();
    }
    guest.input->rest();
    const std::string from = guest.peer.name;
    guest.peer = peer;
    const std::uint16_t port = openStream(hosted, guest, keys);
    lucent::info("remote", "{} continues {} from {} on its pad {}, its stream on UDP port {}",
                 peer.name, hosted.title, from, guest.input->padNode(), port);
    const std::lock_guard lock{*answerMutex_};
    news_->emplace_back(peer.name + " continues " + hosted.title, false);
    return port;
}

std::optional<lan::PlayOutcome> RemotePlay::continueSeat(const lan::Peer& peer,
                                                         const std::string& gameId,
                                                         const std::string& profile,
                                                         const lan::StreamKeys& keys) {
    if (hosted_ ? hosted_->gameId != gameId : !busy_ || runningId_ != gameId) {
        return std::nullopt;
    }
    const auto continued = [](std::uint16_t port, bool joined) {
        return lan::PlayOutcome{
            .port = port, .waiting = false, .joined = joined, .continued = true, .refusal = {}};
    };
    if (hosted_) {
        Hosted& hosted = *hosted_;
        const auto seat = std::ranges::find_if(hosted.guests, [&profile](const auto& guest) {
            return guest->linked && guest->profile == profile;
        });
        if (seat != hosted.guests.end()) {
            const bool started = hosted.forGuest && seat == hosted.guests.begin();
            return continued(takeOver(hosted, **seat, peer, keys), !started);
        }
        if (hosted.forGuest || hosted.guests.size() >= maxGuests) {
            return std::nullopt;
        }
    }
    if (profile != runningProfile_) {
        return std::nullopt;
    }
    if (!hosted_) {
        hostOwnGame(gameId);
    }
    const std::uint16_t port =
        admit(*hosted_,
              Seat{.peer = peer, .profile = profile, .linked = true, .withKeys = true, .own = true},
              keys);
    return continued(port, true);
}

void RemotePlay::hostOwnGame(const std::string& gameId) {
    auto hosted = std::make_unique<Hosted>();
    hosted->gameId = gameId;
    hosted->title = runningTitle_;
    hosted->launched = true;
    hosted->video = std::make_unique<stream::HostVideo>(hosted->audience, screen_.gamescopeSocket);
    hosted->audio = std::make_unique<stream::HostAudio>(hosted->audience, std::string{});
    lucent::info("remote", "streaming {} as it plays here", hosted->title);
    hosted_ = std::move(hosted);
}

lan::PlayOutcome RemotePlay::waitFor(const lan::Peer& peer, const std::string& gameId,
                                     const std::string& profile, bool linked,
                                     const lan::StreamKeys& keys) {
    const auto found = std::ranges::find(asks_, peer.id, [](const Ask& ask) {
        return ask.peer.id;
    });
    if (found != asks_.end()) {
        wipe(found->keys);
        asks_.erase(found);
    }
    const auto sent = std::ranges::find_if(sent_, [&](const Sent& invited) {
        return invited.peerId == peer.id && invited.gameId == gameId;
    });
    const bool invited = sent != sent_.end();
    if (invited) {
        sent_.erase(sent);
    }
    asks_.push_back(Ask{.peer = peer,
                        .gameId = gameId,
                        .profile = profile,
                        .linked = linked,
                        .keys = keys,
                        .at = Clock::now(),
                        .verdict = invited ? Ask::Verdict::Allowed : Ask::Verdict::Waiting,
                        .why = {}});
    lucent::info("remote", "{} {} the game here", peer.name,
                 invited ? "takes its invitation to" : "asks to join");
    return lan::PlayOutcome{
        .port = std::nullopt, .waiting = true, .joined = true, .continued = false, .refusal = {}};
}

lan::PlayOutcome RemotePlay::offer(const lan::Peer& peer, const std::string& gameId,
                                   const std::string& profile, bool linked,
                                   const lan::StreamKeys& keys) {
    const std::lock_guard lock{hostMutex_};
    const auto refuse = [](std::string why) {
        return lan::PlayOutcome{.port = std::nullopt,
                                .waiting = false,
                                .joined = false,
                                .continued = false,
                                .refusal = std::move(why)};
    };
    if (linked) {
        try {
            if (std::optional<lan::PlayOutcome> seat = continueSeat(peer, gameId, profile, keys)) {
                return *seat;
            }
        } catch (const std::runtime_error& failure) {
            lucent::warn("remote", "{} cannot continue here: {}", peer.name, failure.what());
            return refuse(failure.what());
        }
    }
    if (hosted_) {
        Hosted& hosted = *hosted_;
        if (hosted.gameId != gameId) {
            return refuse("it is streaming " + hosted.title + " now");
        }
        const auto held = std::ranges::find_if(hosted.guests, [&peer](const auto& guest) {
            return guest->peer.id == peer.id && !guest->stream && !guest->kicked;
        });
        if (held != hosted.guests.end()) {
            try {
                return lan::PlayOutcome{.port = takeOver(hosted, **held, peer, keys),
                                        .waiting = false,
                                        .joined = true,
                                        .continued = false,
                                        .refusal = {}};
            } catch (const std::runtime_error& failure) {
                lucent::warn("remote", "{} cannot take its seat back: {}", peer.name,
                             failure.what());
                return refuse(failure.what());
            }
        }
        if (std::ranges::any_of(hosted.guests, [&peer](const std::unique_ptr<Guest>& guest) {
                return guest->peer.id == peer.id && guest->stream;
            })) {
            return refuse("this openSU plays it there already");
        }
        if (hosted.guests.size() >= maxGuests &&
            std::ranges::none_of(hosted.guests, [&peer](const std::unique_ptr<Guest>& guest) {
                return guest->peer.id == peer.id;
            })) {
            return refuse(hosted.title + " has four guests there");
        }
        return waitFor(peer, gameId, profile, linked, keys);
    }
    if (screen_.gamescopeSocket.empty()) {
        return refuse("it does not run inside Gamescope, whose screen it would stream");
    }
    if (busy_) {
        if (runningId_.empty() || runningId_ != gameId) {
            return refuse(runningTitle_.empty() ? "a game is already running there"
                                                : "it is playing " + runningTitle_ + " now");
        }
        return waitFor(peer, gameId, profile, linked, keys);
    }
    try {
        auto hosted = std::make_unique<Hosted>();
        hosted->gameId = gameId;
        hosted->profile = profile;
        hosted->forGuest = true;
        const std::uint16_t port = admit(
            *hosted,
            Seat{
                .peer = peer, .profile = profile, .linked = linked, .withKeys = true, .own = false},
            keys);
        hosted->video =
            std::make_unique<stream::HostVideo>(hosted->audience, screen_.gamescopeSocket);
        hosted->audio = std::make_unique<stream::HostAudio>(
            hosted->audience, "opensu.remote." + std::to_string(port));
        hosted_ = std::move(hosted);
        return lan::PlayOutcome{
            .port = port, .waiting = false, .joined = false, .continued = false, .refusal = {}};
    } catch (const std::runtime_error& failure) {
        lucent::warn("remote", "cannot stream to {}: {}", peer.name, failure.what());
        return refuse(failure.what());
    }
}

std::optional<lan::wire::PlayVerdict> RemotePlay::playStatus(const lan::Peer& peer,
                                                             const std::string& gameId) {
    const std::lock_guard lock{hostMutex_};
    lapseAsks();
    const auto found = std::ranges::find_if(asks_, [&](const Ask& ask) {
        return ask.peer.id == peer.id && ask.gameId == gameId;
    });
    if (found == asks_.end()) {
        return std::nullopt;
    }
    lan::wire::PlayVerdict verdict{.verdict = "waiting", .port = 0, .why = {}};
    if (found->verdict == Ask::Verdict::Waiting) {
        return verdict;
    }
    Ask ask = std::move(*found);
    asks_.erase(found);
    verdict.verdict = "declined";
    if (ask.verdict == Ask::Verdict::Allowed) {
        try {
            if (!hosted_) {
                hostOwnGame(ask.gameId);
            }
            const auto held = std::ranges::find_if(hosted_->guests, [&ask](const auto& guest) {
                return guest->peer.id == ask.peer.id && !guest->stream;
            });
            if (hosted_->gameId != ask.gameId) {
                verdict.why = "it is streaming " + hosted_->title + " now";
            } else if (held != hosted_->guests.end()) {
                verdict.port = takeOver(*hosted_, **held, ask.peer, ask.keys);
                verdict.verdict = "accepted";
            } else if (hosted_->guests.size() >= maxGuests) {
                verdict.why = hosted_->title + " has four guests there";
            } else {
                verdict.port = admit(*hosted_,
                                     Seat{.peer = ask.peer,
                                          .profile = ask.profile,
                                          .linked = ask.linked,
                                          .withKeys = false,
                                          .own = false},
                                     ask.keys);
                verdict.verdict = "accepted";
            }
        } catch (const std::runtime_error& failure) {
            lucent::warn("remote", "cannot let {} join: {}", peer.name, failure.what());
            verdict.why = failure.what();
            if (hosted_ && hosted_->guests.empty()) {
                hosted_.reset();
            }
        }
    }
    wipe(ask.keys);
    lucent::info("remote", "told {} its ask is {}{}{}", peer.name, verdict.verdict,
                 verdict.why.empty() ? "" : ": ", verdict.why);
    return verdict;
}

void RemotePlay::lapseAsks() {
    const Clock::time_point now = Clock::now();
    for (Ask& ask : asks_) {
        if (ask.verdict == Ask::Verdict::Waiting && now - ask.at > askPatience) {
            ask.verdict = Ask::Verdict::Declined;
            ask.why = "its player did not answer";
            ask.at = now;
        }
    }
    std::erase_if(sent_, [this, now](const Sent& invited) {
        return now - invited.at > askPatience || invited.gameId != runningId_;
    });
    std::erase_if(received_, [now](const Invited& invited) {
        return now - invited.at > askPatience;
    });
    std::erase_if(asks_, [now](Ask& ask) {
        const bool gone = ask.verdict != Ask::Verdict::Waiting && now - ask.at > answerKept;
        if (gone) {
            wipe(ask.keys);
        }
        return gone;
    });
}

std::optional<RemotePlay::JoinQuestion> RemotePlay::joinQuestion() const {
    const std::lock_guard lock{hostMutex_};
    for (const Ask& ask : asks_) {
        if (ask.verdict == Ask::Verdict::Waiting) {
            return JoinQuestion{.peer = ask.peer,
                                .title = hosted_ ? hosted_->title : runningTitle_};
        }
    }
    return std::nullopt;
}

void RemotePlay::answerJoin(const std::string& peerId, bool allow) {
    const std::lock_guard lock{hostMutex_};
    const auto found = std::ranges::find_if(asks_, [&peerId](const Ask& ask) {
        return ask.peer.id == peerId && ask.verdict == Ask::Verdict::Waiting;
    });
    if (found == asks_.end()) {
        return;
    }
    found->verdict = allow ? Ask::Verdict::Allowed : Ask::Verdict::Declined;
    found->at = Clock::now();
    lucent::info("remote", "the player {} {} to join", allow ? "allows" : "declines",
                 found->peer.name);
    hooks_.say(allow ? found->peer.name + " joins" : "Declined " + found->peer.name, false);
}

bool RemotePlay::takesGuests() const {
    const std::lock_guard lock{hostMutex_};
    return !screen_.gamescopeSocket.empty() && !runningId_.empty() &&
           (!hosted_ || hosted_->guests.size() < maxGuests);
}

std::vector<lan::Peer> RemotePlay::guests() const {
    const std::lock_guard lock{hostMutex_};
    std::vector<lan::Peer> peers;
    if (hosted_) {
        std::ranges::transform(hosted_->guests, std::back_inserter(peers),
                               [](const std::unique_ptr<Guest>& guest) {
                                   return guest->peer;
                               });
    }
    return peers;
}

std::vector<lan::Peer> RemotePlay::notGuests(std::vector<lan::Peer> peers) const {
    const std::vector<lan::Peer> playing = guests();
    std::erase_if(peers, [&playing](const lan::Peer& peer) {
        return std::ranges::find(playing, peer.id, &lan::Peer::id) != playing.end();
    });
    return peers;
}

bool RemotePlay::closesWithGuest() const {
    const std::lock_guard lock{hostMutex_};
    return hosted_ && hosted_->forGuest && hosted_->guests.size() == 1;
}

void RemotePlay::invite(const lan::Peer& peer) {
    const lan::wire::Invitation game = [&] {
        const std::lock_guard lock{hostMutex_};
        std::erase_if(sent_, [&peer](const Sent& invited) {
            return invited.peerId == peer.id;
        });
        sent_.push_back(Sent{.peerId = peer.id, .gameId = runningId_, .at = Clock::now()});
        return lan::wire::Invitation{runningId_, runningTitle_};
    }();
    const bool told = hooks_.invite(
        peer.id, game,
        [mutex = answerMutex_, news = news_, name = peer.name](const std::string& why) {
            const std::lock_guard lock{*mutex};
            news->emplace_back(name + " cannot join: " + why, true);
        });
    if (!told) {
        hooks_.say("Cannot invite " + peer.name + " now: LAN is off or busy", true);
        return;
    }
    lucent::info("remote", "invited {} to {}", peer.name, game.title);
    hooks_.say("Invited " + peer.name + " to " + game.title, false);
}

void RemotePlay::kick(const std::string& peerId) {
    const std::lock_guard lock{hostMutex_};
    if (!hosted_) {
        return;
    }
    const auto found = std::ranges::find_if(hosted_->guests, [&peerId](const auto& guest) {
        return guest->peer.id == peerId;
    });
    if (found == hosted_->guests.end()) {
        return;
    }
    (*found)->kicked = true;
    if ((*found)->stream) {
        (*found)->stream->end(hooks_.name() + " removed you");
    }
    lucent::info("remote", "the player removed {}", (*found)->peer.name);
    hooks_.say("Removed " + (*found)->peer.name, false);
}

std::optional<std::string> RemotePlay::invited(const lan::Peer& peer,
                                               const lan::wire::Invitation& game) {
    if (occupied_) {
        return "it is in a game";
    }
    const std::lock_guard lock{hostMutex_};
    std::erase_if(received_, [&peer](const Invited& invited) {
        return invited.peer.id == peer.id;
    });
    received_.push_back(
        Invited{.peer = peer, .gameId = game.game, .title = game.title, .at = Clock::now()});
    lucent::info("remote", "{} invites this openSU to {}", peer.name, game.title);
    return std::nullopt;
}

std::optional<RemotePlay::Invited> RemotePlay::invitation() const {
    const std::lock_guard lock{hostMutex_};
    return received_.empty() ? std::nullopt : std::optional{received_.front()};
}

void RemotePlay::answerInvitation(const std::string& hostId, bool join) {
    std::optional<Invited> taken;
    {
        const std::lock_guard lock{hostMutex_};
        const auto found = std::ranges::find_if(received_, [&hostId](const Invited& invited) {
            return invited.peer.id == hostId;
        });
        if (found == received_.end()) {
            return;
        }
        taken = std::move(*found);
        received_.erase(found);
    }
    if (join && !hooks_.join(taken->peer.id, taken->gameId)) {
        hooks_.say(taken->title + " is not in this Library yet", true);
    }
}

void RemotePlay::start(const library::Game& game, stream::wire::PadMapping mapping,
                       stream::PictureMemory pictures) {
    if (playing()) {
        hooks_.say("remote play is already on", true);
        return;
    }
    game_ = game;
    mapping_ = std::move(mapping);
    pictures_ = pictures;
    waiting_->store(false);
    saidWaiting_ = false;
    asking_ = hooks_.ask(
        game.hostId, game.sourceId,
        [waiting = waiting_] {
            waiting->store(true);
        },
        [mutex = answerMutex_, answer = answer_](std::optional<lan::LanNode::PlayOffer> offer,
                                                 std::string error) {
            const std::lock_guard lock{*mutex};
            *answer = Answer{std::move(offer), std::move(error)};
        });
    if (!asking_) {
        hooks_.say("Cannot ask " + game.hostName + " now: LAN is off or busy", true);
        return;
    }
    lucent::info("remote", "asking {} to stream {}", game.hostName, game.title);
}

std::string RemotePlay::stage() const {
    if (cancelled_) {
        return {};
    }
    if (waiting()) {
        return "Waiting for " + game_.hostName + " to let you in";
    }
    if (asking_) {
        return "Asking " + game_.hostName + " to start the game";
    }
    if (guest_ && !pictured_) {
        const std::string step = guest_->stage();
        return step.empty() ? "Waiting for " + game_.hostName + "'s picture" : step;
    }
    return {};
}

void RemotePlay::cancel() {
    if (asking_) {
        cancelled_ = true;
    } else {
        end();
    }
}

void RemotePlay::end() {
    if (guest_) {
        guest_->end(joined_ ? "the player left" : "the player ended remote play");
        drop();
        hooks_.say((joined_ ? "Left " : "Remote play of ") + game_.title +
                       (joined_ ? " on " + game_.hostName : " ended"),
                   false);
    }
}

void RemotePlay::hostStage(std::string text) {
    const std::lock_guard lock{hostMutex_};
    if (hosted_) {
        hosted_->stage = std::move(text);
        hosted_->stageSent = {};
    }
}

void RemotePlay::service(bool busy, const library::Game* running, const std::string& profile,
                         bool shown) {
    busy_ = busy;
    shown_ = shown;
    occupied_ = busy || playing();
    {
        const std::lock_guard lock{*answerMutex_};
        for (const auto& [text, error] : *news_) {
            hooks_.say(text, error);
        }
        news_->clear();
    }
    serviceHost(running, profile);
    serviceGuest();
}

RemotePlay::GuestList::iterator RemotePlay::free(Hosted& hosted, GuestList::iterator at) {
    const bool own = (*at)->own;
    if ((*at)->stream) {
        hosted.audience.remove(*(*at)->stream);
    }
    GuestList::iterator next = hosted.guests.erase(at);
    if (own) {
        hooks_.returnPad();
    }
    return next;
}

void RemotePlay::hold(Hosted& hosted, Guest& guest) {
    hosted.audience.remove(*guest.stream);
    guest.stream.reset();
    guest.input->rest();
    guest.heldSince = Clock::now();
    lucent::info("remote", "{} went quiet; its seat in {} is held for {} s", guest.peer.name,
                 hosted.title, seatHold.count());
    hooks_.say(guest.peer.name + " dropped; their seat is held", false);
}

void RemotePlay::publishSeats() {
    std::pair<std::string, std::vector<std::string>> seats{runningId_, {}};
    if (!runningId_.empty()) {
        if (!hosted_ || !hosted_->forGuest) {
            seats.second.push_back(runningProfile_);
        }
        if (hosted_) {
            for (const std::unique_ptr<Guest>& guest : hosted_->guests) {
                if (guest->linked) {
                    seats.second.push_back(guest->profile);
                }
            }
        }
        std::ranges::sort(seats.second);
        const auto [gone, end] = std::ranges::unique(seats.second);
        seats.second.erase(gone, end);
    }
    if (seats != published_) {
        published_ = seats;
        hooks_.seats(std::move(seats.first), std::move(seats.second));
    }
}

void RemotePlay::serviceHost(const library::Game* running, const std::string& profile) {
    const std::lock_guard lock{hostMutex_};
    runningId_ = running != nullptr ? running->id : std::string{};
    runningTitle_ = running != nullptr ? running->title : std::string{};
    runningProfile_ = running != nullptr ? profile : std::string{};
    lapseAsks();
    publishSeats();
    if (!hosted_) {
        return;
    }
    Hosted& hosted = *hosted_;
    if (hosted.forGuest && !hosted.launched && !hosted.guests.empty()) {
        const std::optional<library::Game> game = hooks_.find(hosted.gameId);
        Guest& first = *hosted.guests.front();
        if (!game) {
            first.stream->end("the game is no longer installed there");
        } else if (busy_) {
            first.stream->end("a game is already running there");
        } else {
            hosted.title = game->title;
            lucent::info("remote", "{} plays {} from here", first.peer.name, game->title);
            if (!hooks_.launch(*game, hosted.audio->environment(), hosted.profile)) {
                first.stream->end("the game could not start there");
            } else {
                hosted.launched = true;
                hooks_.say(first.peer.name + " plays " + game->title + " from here", false);
                return;
            }
        }
    }
    if (hooks_.facts && Clock::now() - hosted.factsSent >= factsEvery) {
        hosted.factsSent = Clock::now();
        tellGuests(hosted, hooks_.facts());
    }
    const bool held = hosted.forGuest && !shown_;
    if (held && !hosted.stage.empty() && Clock::now() - hosted.stageSent >= stageEvery) {
        hosted.stageSent = Clock::now();
        for (const std::unique_ptr<Guest>& guest : hosted.guests) {
            if (guest->stream) {
                guest->stream->send(stream::PacketKind::Stage, hosted.stage);
            }
        }
    }
    if (hosted.video) {
        hosted.video->hold(held);
        try {
            hosted.video->fit();
        } catch (const std::runtime_error& failure) {
            for (const std::unique_ptr<Guest>& guest : hosted.guests) {
                if (guest->stream) {
                    guest->stream->end(failure.what());
                }
            }
        }
    }
    // A launch that is taken runs, or waits for its saves, from that frame on.
    const bool on =
        !hosted.launched || (hosted.forGuest ? busy_.load() : runningId_ == hosted.gameId);
    if (!on) {
        for (const std::unique_ptr<Guest>& guest : hosted.guests) {
            if (guest->stream) {
                guest->stream->end("the game ended");
            }
        }
    }
    std::vector<std::pair<std::string, std::string>> left;
    for (auto it = hosted.guests.begin(); it != hosted.guests.end();) {
        Guest& guest = **it;
        if (!guest.stream) {
            if (on && !guest.kicked && Clock::now() - guest.heldSince < seatHold) {
                ++it;
                continue;
            }
            lucent::info("remote", "the seat {} held in {} is free", guest.peer.name, hosted.title);
            it = free(hosted, it);
            continue;
        }
        const std::optional<std::string> over = guest.stream->over();
        if (!over) {
            ++it;
            continue;
        }
        if (on && !guest.kicked && guest.stream->dropped()) {
            hold(hosted, guest);
            ++it;
            continue;
        }
        lucent::info("remote", "{} left {}: {}", (*it)->peer.name, hosted.title, *over);
        if (!(*it)->kicked && on) {
            left.emplace_back((*it)->peer.name, *over);
        }
        it = free(hosted, it);
    }
    for (const auto& [name, why] : left) {
        hooks_.say(hosted.forGuest && hosted.guests.empty() && left.size() == 1
                       ? std::format("Remote play with {} ended: {}", name, why)
                       : name + " left",
                   false);
    }
    if (!hosted.guests.empty()) {
        return;
    }
    if (hosted.forGuest && hosted.launched && busy_) {
        hooks_.close();
    }
    for (Ask& ask : asks_) {
        if (hosted.forGuest && ask.verdict == Ask::Verdict::Waiting &&
            ask.gameId == hosted.gameId) {
            ask.verdict = Ask::Verdict::Declined;
            ask.why = "the game ended there";
            ask.at = Clock::now();
        }
    }
    hosted_.reset();
}

void RemotePlay::serviceGuest() {
    if (asking_) {
        if (waiting_->load() && !saidWaiting_) {
            saidWaiting_ = true;
            hooks_.say("Waiting for " + game_.hostName + " to let you in", false);
        }
        std::optional<Answer> answer;
        {
            const std::lock_guard lock{*answerMutex_};
            answer = std::exchange(*answer_, std::nullopt);
        }
        if (!answer) {
            return;
        }
        asking_ = false;
        waiting_->store(false);
        // The host gives up on a guest that does not join.
        if (std::exchange(cancelled_, false)) {
            return;
        }
        if (!answer->offer) {
            lucent::warn("remote", "{} does not stream {}: {}", game_.hostName, game_.title,
                         answer->error);
            hooks_.say(game_.hostName + " cannot stream " + game_.title + ": " + answer->error,
                       true);
            return;
        }
        const std::optional<stream::UdpAddress> host =
            stream::udpAddress(answer->offer->stream.address, answer->offer->stream.port);
        if (!host) {
            hooks_.say(game_.hostName + " is not at an IPv4 address", true);
            return;
        }
        generation_ = hooks_.frameGen ? hooks_.frameGen(game_) : settings::FrameGenSettings{};
        try {
            link_ = std::make_unique<stream::LinkHold>();
            // The generator takes CPU RGBA.
            video_ = std::make_unique<stream::GuestVideo>(
                generation_.mode == settings::FrameGenMode::Off ? pictures_
                                                                : stream::PictureMemory::Cpu);
            speaker_ = hooks_.speaker ? hooks_.speaker() : nullptr;
            audio_ = std::make_unique<stream::GuestAudio>(
                [speaker = speaker_.get()](std::span<const float> samples) {
                    if (speaker != nullptr) {
                        speaker->feed(samples);
                    }
                });
            guest_ = std::make_unique<stream::GuestStream>(stream::GuestStream::Setup{
                .host = *host,
                .keys = std::move(answer->offer->keys),
                .video =
                    [video = video_.get()](const stream::wire::VideoPacket& packet,
                                           std::uint64_t receivedUs, std::int64_t hostAheadUs) {
                        return video->take(packet, receivedUs, hostAheadUs);
                    },
                .audio =
                    [sound = audio_.get()](const stream::wire::AudioPacket& packet,
                                           std::uint64_t receivedUs, std::int64_t hostAheadUs) {
                        sound->take(packet, receivedUs, hostAheadUs);
                    },
                .decoders = stream::decodableHere()});
            guest_->mapping(mapping_);
            guest_->capLink(linkCap_);
            guest_->wantRate(hooks_.streamRate ? hooks_.streamRate(game_) : 0);
        } catch (const std::runtime_error& failure) {
            drop();
            lucent::warn("remote", "remote play cannot start: {}", failure.what());
            hooks_.say("Remote play cannot start: " + std::string{failure.what()}, true);
            return;
        }
        joined_ = answer->offer->joined;
        const bool continued = answer->offer->continued;
        lucent::info("remote", "{} {} on {} at {}:{}", joined_ ? "joined" : "playing", game_.title,
                     game_.hostName, answer->offer->stream.address, answer->offer->stream.port);
        if (continued) {
            hooks_.say("Continued " + game_.title + " here", false);
        } else if (joined_) {
            hooks_.say("Joined " + game_.title + " on " + game_.hostName, false);
        }
        if (generation_.mode != settings::FrameGenMode::Off &&
            generation_.generator == settings::FrameGenGenerator::OpenSU) {
            startFrameGen(std::string{}, {}, "openSU");
        } else if (generation_.mode != settings::FrameGenMode::Off) {
            // A slot of this session's own, so an answer to an earlier one lands elsewhere.
            modules_ = std::make_shared<
                std::optional<std::pair<std::optional<std::string>, std::string>>>();
            modulesAsked_ =
                hooks_.frameGenModules &&
                hooks_.frameGenModules(
                    game_.hostId, [mutex = modulesMutex_, answer = modules_](
                                      std::optional<std::string> modules, std::string error) {
                        const std::lock_guard lock{*mutex};
                        *answer = std::pair{std::move(modules), std::move(error)};
                    });
            if (!modulesAsked_) {
                frameGenOff_ = "LAN is off or busy";
                lucent::warn("framegen", "frame generation is off: cannot ask {} for its shaders",
                             game_.hostName);
            }
        }
        return;
    }
    serviceFrameGen();
    if (guest_) {
        if (const std::optional<std::string> over = guest_->over()) {
            hooks_.say(game_.title + " on " + game_.hostName + ": " + *over, !pictured_);
            drop();
        }
    }
}

void RemotePlay::serviceFrameGen() {
    if (!modulesAsked_ || !guest_) {
        return;
    }
    std::optional<std::pair<std::optional<std::string>, std::string>> answer;
    {
        const std::lock_guard lock{*modulesMutex_};
        answer = std::exchange(*modules_, std::nullopt);
    }
    if (!answer) {
        return;
    }
    modulesAsked_ = false;
    startFrameGen(std::move(answer->first), std::move(answer->second),
                  game_.hostName + "'s shaders");
}

void RemotePlay::startFrameGen(std::optional<std::string> packed, std::string why,
                               const std::string& by) {
    if (packed) {
        frameGen_ = RemoteFrameGen::open(generation_, *packed, *video_, why);
    }
    if (!frameGen_) {
        frameGenOff_ = why;
        lucent::warn("framegen", "frame generation for {} is off: {}", game_.title, why);
        hooks_.say("Frame generation is off: " + why, true);
        return;
    }
    lucent::info("framegen", "frame generation for {} by {}", game_.title, by);
}

void RemotePlay::drop() {
    frameGen_.reset();
    frameGenOff_.clear();
    modulesAsked_ = false;
    guest_.reset();
    audio_.reset();
    speaker_.reset();
    video_.reset();
    link_.reset();
    joined_ = false;
    pictured_ = false;
}

void RemotePlay::tellGuests(Hosted& hosted, const GameFacts& facts) {
    if (hosted.forGuest && hosted.audio) {
        // Steam starts its games itself, so they miss the sink's environment.
        if (const std::optional<library::Game> game = hooks_.find(hosted.gameId)) {
            hosted.audio->adopt(launch::ProcessTree::treesMatching(game->processHint));
        }
    }
    const stream::PictureSize picture = facts.picture;
    const bool lowRes = picture.width > 0 && picture.height > 0 &&
                        std::max(picture.width, picture.height) <= lowResSide;
    if (hosted.forGuest && lowRes && hosted.video && picture != hosted.native) {
        lucent::info("remote", "a {}x{} picture goes at its own size", picture.width,
                     picture.height);
        hosted.native = picture;
        hooks_.nativeWindow();
        hosted.video->native(picture);
    }
    for (const std::unique_ptr<Guest>& guest : hosted.guests) {
        if (!guest->stream) {
            continue;
        }
        if (!facts.panel.empty()) {
            guest->stream->send(stream::PacketKind::Panel, facts.panel);
        }
        if (facts.runner) {
            const float shape = hosted.native.width > 0 ? facts.aspect : 0.0F;
            guest->stream->send(
                stream::PacketKind::Shape,
                std::format("{} {}", std::lround(shape * 10000.0F), facts.nearest ? 1 : 0));
        }
    }
}

std::optional<bool> RemotePlay::nearest() const {
    return guest_ ? guest_->nearest() : std::nullopt;
}

float RemotePlay::shape() const {
    return guest_ ? guest_->shape() : 0.0F;
}

std::string RemotePlay::panel() const {
    return guest_ ? guest_->panel() : std::string{};
}

void RemotePlay::showsIn(int width, int height) {
    if (guest_) {
        guest_->showsIn(width, height);
    }
}

void RemotePlay::input(const stream::wire::PadState& pad, const stream::wire::KeyState& keys,
                       const stream::wire::MouseState& mouse) {
    if (guest_) {
        guest_->pad(pad);
        guest_->keys(keys);
        guest_->mouse(mouse);
    }
}

void RemotePlay::capLink(std::uint64_t bitsPerSecond) {
    linkCap_ = bitsPerSecond;
    if (guest_) {
        guest_->capLink(bitsPerSecond);
    }
    lucent::info("remote", "a test run caps the link at {} bit/s", bitsPerSecond);
}

void RemotePlay::readPicturesBack() {
    pictures_ = stream::PictureMemory::Cpu;
    if (video_) {
        video_->readBack();
    }
}

std::optional<stream::GuestVideo::Frame> RemotePlay::takePicture(std::uint64_t nowUs,
                                                                 std::uint64_t refreshNs) {
    if (!video_) {
        return std::nullopt;
    }
    if (frameGen_ && frameGen_->failed()) {
        frameGen_.reset();
        frameGenOff_ = "it failed; see the log";
        hooks_.say("Frame generation stopped", true);
    }
    // The generator takes the frames itself as they are decoded.
    std::optional<stream::GuestVideo::Frame> frame =
        frameGen_ ? frameGen_->present(nowUs, refreshNs) : video_->takeFrame();
    pictured_ = pictured_ || frame.has_value();
    return frame;
}

void RemotePlay::presented(std::uint32_t number, std::uint64_t submittedUs, bool drawn) {
    if (video_) {
        video_->presented(number, submittedUs, drawn);
    }
}

std::optional<stream::GuestVideo::Frame> RemotePlay::picture() const {
    if (video_) {
        return video_->lastDecoded();
    }
    const std::lock_guard lock{hostMutex_};
    if (!hosted_ || !hosted_->video) {
        return std::nullopt;
    }
    std::optional<std::pair<std::uint32_t, stream::Picture>> sent = hosted_->video->lastSent();
    if (!sent) {
        return std::nullopt;
    }
    return stream::GuestVideo::Frame{
        .number = sent->first, .picture = std::move(sent->second), .capturedUs = 0, .decodedUs = 0};
}

RemotePlay::Glance RemotePlay::glance() const {
    Glance glance;
    if (asking_) {
        glance.state = waiting_->load() ? "waiting" : "asking";
        glance.peer = game_.hostName;
    } else if (guest_) {
        const stream::GuestStream::Timing timing = guest_->timing();
        glance.state = "playing";
        glance.peer = game_.hostName;
        glance.roundTripUs = timing.roundTripUs;
        glance.bytesPerSecond = timing.bytesPerSecond;
        glance.video = video_->summary();
        if (frameGen_) {
            glance.video += " " + frameGen_->summary();
        } else if (!frameGenOff_.empty()) {
            glance.video += " framegen off: " + frameGenOff_;
        }
        glance.audio = audio_->summary();
    }
    const std::lock_guard lock{hostMutex_};
    for (const Ask& ask : asks_) {
        if (ask.verdict == Ask::Verdict::Waiting) {
            glance.asking = ask.peer.name;
            break;
        }
    }
    if (glance.state.empty() && hosted_) {
        glance.state = "hosting";
        glance.guests = hosted_->guests.size();
        std::vector<std::string> names;
        for (const std::unique_ptr<Guest>& guest : hosted_->guests) {
            names.push_back(guest->peer.name);
            if (!guest->stream) {
                ++glance.held;
                continue;
            }
            if (!glance.video.empty()) {
                glance.video += " | ";
            }
            glance.video += guest->peer.name + ": " + guest->stream->linkSummary();
        }
        glance.peer = listOf(names);
    }
    return glance;
}

} // namespace opensu::app

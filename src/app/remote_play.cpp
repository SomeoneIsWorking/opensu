#include "remote_play.hpp"

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "lucent/log.h"
#include "secrets/secret.hpp"
#include "stream/udp_socket.hpp"

namespace opensu::app {
namespace {

using Clock = std::chrono::steady_clock;

/// How long an answer waits for the guest to ask how its ask stands.
constexpr std::chrono::seconds answerKept{30};

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
            guest->stream->end("openSU closed");
        }
    }
    for (Ask& ask : asks_) {
        wipe(ask.keys);
    }
}

std::uint16_t RemotePlay::admit(Hosted& hosted, const lan::Peer& peer, const lan::StreamKeys& keys,
                                bool withKeys) {
    auto guest = std::make_unique<Guest>();
    guest->peer = peer;
    guest->input = std::make_unique<stream::HostInput>(withKeys ? std::optional{screen_.x11Display}
                                                                : std::nullopt);
    stream::HostInput& player = *guest->input;
    guest->stream = std::make_unique<stream::HostStream>(
        stream::HostStream::Setup{.keys = keys,
                                  .guest = peer.name,
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
    const std::uint16_t port = guest->stream->port();
    lucent::info("remote", "{} is guest {} of {}, its pad {}, its stream on UDP port {}", peer.name,
                 hosted.guests.size() + 1, hosted.title.empty() ? hosted.gameId : hosted.title,
                 player.padNode(), port);
    hosted.audience.add(*guest->stream);
    hosted.guests.push_back(std::move(guest));
    return port;
}

void RemotePlay::hostOwnGame(const Ask& ask) {
    auto hosted = std::make_unique<Hosted>();
    hosted->gameId = ask.gameId;
    hosted->title = runningTitle_;
    hosted->launched = true;
    hosted->video = std::make_unique<stream::HostVideo>(hosted->audience, screen_.gamescopeSocket);
    hosted->audio = std::make_unique<stream::HostAudio>(hosted->audience, std::string{});
    lucent::info("remote", "streaming {} as it plays here", hosted->title);
    hosted_ = std::move(hosted);
}

lan::PlayOutcome RemotePlay::waitFor(const lan::Peer& peer, const std::string& gameId,
                                     const lan::StreamKeys& keys) {
    const auto found = std::ranges::find(asks_, peer.id, [](const Ask& ask) {
        return ask.peer.id;
    });
    if (found != asks_.end()) {
        wipe(found->keys);
        asks_.erase(found);
    }
    asks_.push_back(Ask{.peer = peer,
                        .gameId = gameId,
                        .keys = keys,
                        .at = Clock::now(),
                        .verdict = Ask::Verdict::Waiting,
                        .why = {}});
    lucent::info("remote", "{} asks to join the game here", peer.name);
    return lan::PlayOutcome{.port = std::nullopt, .waiting = true, .joined = true, .refusal = {}};
}

lan::PlayOutcome RemotePlay::offer(const lan::Peer& peer, const std::string& gameId,
                                   const std::string& profile, const lan::StreamKeys& keys) {
    const std::lock_guard lock{hostMutex_};
    const auto refuse = [](std::string why) {
        return lan::PlayOutcome{
            .port = std::nullopt, .waiting = false, .joined = false, .refusal = std::move(why)};
    };
    if (hosted_) {
        const Hosted& hosted = *hosted_;
        if (hosted.gameId != gameId) {
            return refuse("it is streaming " + hosted.title + " now");
        }
        if (std::ranges::any_of(hosted.guests, [&peer](const std::unique_ptr<Guest>& guest) {
                return guest->peer.id == peer.id;
            })) {
            return refuse("this openSU plays it there already");
        }
        if (hosted.guests.size() >= maxGuests) {
            return refuse(hosted.title + " has four guests there");
        }
        return waitFor(peer, gameId, keys);
    }
    if (screen_.gamescopeSocket.empty()) {
        return refuse("it does not run inside Gamescope, whose screen it would stream");
    }
    if (busy_) {
        if (runningId_.empty() || runningId_ != gameId) {
            return refuse(runningTitle_.empty() ? "a game is already running there"
                                                : "it is playing " + runningTitle_ + " now");
        }
        return waitFor(peer, gameId, keys);
    }
    try {
        auto hosted = std::make_unique<Hosted>();
        hosted->gameId = gameId;
        hosted->profile = profile;
        hosted->forGuest = true;
        const std::uint16_t port = admit(*hosted, peer, keys, true);
        hosted->video =
            std::make_unique<stream::HostVideo>(hosted->audience, screen_.gamescopeSocket);
        hosted->audio = std::make_unique<stream::HostAudio>(
            hosted->audience, "opensu.remote." + std::to_string(port));
        hosted_ = std::move(hosted);
        return lan::PlayOutcome{.port = port, .waiting = false, .joined = false, .refusal = {}};
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
                hostOwnGame(ask);
            }
            if (hosted_->gameId != ask.gameId) {
                verdict.why = "it is streaming " + hosted_->title + " now";
            } else if (hosted_->guests.size() >= maxGuests) {
                verdict.why = hosted_->title + " has four guests there";
            } else {
                verdict.port = admit(*hosted_, ask.peer, ask.keys, false);
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

void RemotePlay::start(const library::Game& game, stream::wire::PadMapping mapping) {
    if (playing()) {
        hooks_.say("remote play is already on", true);
        return;
    }
    game_ = game;
    mapping_ = std::move(mapping);
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
    hooks_.say("Starting " + game.title + " on " + game.hostName, false);
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

void RemotePlay::service(bool busy, const library::Game* running) {
    busy_ = busy;
    serviceHost(running);
    serviceGuest();
}

void RemotePlay::serviceHost(const library::Game* running) {
    const std::lock_guard lock{hostMutex_};
    runningId_ = running != nullptr ? running->id : std::string{};
    runningTitle_ = running != nullptr ? running->title : std::string{};
    lapseAsks();
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
    } else if (hosted.launched) {
        // A launch that is taken runs, or waits for its saves, from that frame on.
        const bool on = hosted.forGuest ? busy_.load() : runningId_ == hosted.gameId;
        if (!on) {
            for (const std::unique_ptr<Guest>& guest : hosted.guests) {
                guest->stream->end("the game ended");
            }
        }
    }
    std::vector<std::pair<std::string, std::string>> left;
    for (auto it = hosted.guests.begin(); it != hosted.guests.end();) {
        const std::optional<std::string> over = (*it)->stream->over();
        if (!over) {
            ++it;
            continue;
        }
        lucent::info("remote", "{} left {}: {}", (*it)->peer.name, hosted.title, *over);
        left.emplace_back((*it)->peer.name, *over);
        hosted.audience.remove(*(*it)->stream);
        it = hosted.guests.erase(it);
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
        try {
            video_ = std::make_unique<stream::GuestVideo>();
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
        } catch (const std::runtime_error& failure) {
            drop();
            lucent::warn("remote", "remote play cannot start: {}", failure.what());
            hooks_.say("Remote play cannot start: " + std::string{failure.what()}, true);
            return;
        }
        joined_ = answer->offer->joined;
        lucent::info("remote", "{} {} on {} at {}:{}", joined_ ? "joined" : "playing", game_.title,
                     game_.hostName, answer->offer->stream.address, answer->offer->stream.port);
        if (joined_) {
            hooks_.say("Joined " + game_.title + " on " + game_.hostName, false);
        }
        return;
    }
    if (guest_) {
        if (const std::optional<std::string> over = guest_->over()) {
            hooks_.say(game_.title + " on " + game_.hostName + " ended: " + *over, false);
            drop();
        }
    }
}

void RemotePlay::drop() {
    guest_.reset();
    audio_.reset();
    speaker_.reset();
    video_.reset();
    joined_ = false;
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

std::optional<stream::GuestVideo::Frame> RemotePlay::takePicture() {
    return video_ ? video_->takeFrame() : std::nullopt;
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
    return stream::GuestVideo::Frame{.number = sent->first, .picture = std::move(sent->second)};
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

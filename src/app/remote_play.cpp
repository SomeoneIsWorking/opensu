#include "remote_play.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "lucent/log.h"
#include "stream/udp_socket.hpp"

namespace opensu::app {

RemotePlay::RemotePlay(Screen screen, Hooks hooks)
    : screen_{std::move(screen)}, hooks_{std::move(hooks)} {
}

RemotePlay::~RemotePlay() {
    if (guest_) {
        guest_->end("openSU closed");
    }
    const std::lock_guard lock{hostMutex_};
    if (hosted_) {
        hosted_->stream->end("openSU closed");
    }
}

std::optional<std::uint16_t> RemotePlay::offer(const lan::Peer& peer, const std::string& gameId,
                                               const std::string& profile,
                                               const lan::StreamKeys& keys, std::string& refusal) {
    const std::lock_guard lock{hostMutex_};
    if (hosted_) {
        refusal = "it is already streaming to " + hosted_->guest;
        return std::nullopt;
    }
    if (busy_) {
        refusal = "a game is already running there";
        return std::nullopt;
    }
    if (screen_.gamescopeSocket.empty()) {
        refusal = "it does not run inside Gamescope, whose screen it would stream";
        return std::nullopt;
    }
    try {
        auto input = std::make_unique<stream::HostInput>(screen_.x11Display);
        stream::HostInput& player = *input;
        auto stream = std::make_unique<stream::HostStream>(
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
        auto video = std::make_unique<stream::HostVideo>(*stream, screen_.gamescopeSocket);
        const std::uint16_t port = stream->port();
        auto sound =
            std::make_unique<stream::HostAudio>(*stream, "opensu.remote." + std::to_string(port));
        hosted_ = Hosted{.input = std::move(input),
                         .stream = std::move(stream),
                         .video = std::move(video),
                         .audio = std::move(sound),
                         .guest = peer.name,
                         .gameId = gameId,
                         .profile = profile};
        return port;
    } catch (const std::runtime_error& failure) {
        lucent::warn("remote", "cannot stream to {}: {}", peer.name, failure.what());
        refusal = failure.what();
        return std::nullopt;
    }
}

void RemotePlay::start(const library::Game& game, stream::wire::PadMapping mapping) {
    if (playing()) {
        hooks_.say("remote play is already on", true);
        return;
    }
    game_ = game;
    mapping_ = std::move(mapping);
    asking_ = hooks_.ask(game.hostId, game.sourceId,
                         [mutex = answerMutex_, answer = answer_](
                             std::optional<lan::LanNode::PlayOffer> offer, std::string error) {
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
        guest_->end("the player ended remote play");
        drop();
        hooks_.say("Remote play of " + game_.title + " ended", false);
    }
}

void RemotePlay::service(bool busy) {
    busy_ = busy;
    serviceHost();
    serviceGuest();
}

void RemotePlay::serviceHost() {
    const std::lock_guard lock{hostMutex_};
    if (!hosted_) {
        return;
    }
    Hosted& hosted = *hosted_;
    if (const std::optional<std::string> over = hosted.stream->over()) {
        if (hosted.launched && busy_) {
            hooks_.close();
        }
        hooks_.say("Remote play with " + hosted.guest + " ended: " + *over, false);
        hosted_.reset();
        return;
    }
    if (!hosted.launched) {
        const std::optional<library::Game> game = hooks_.find(hosted.gameId);
        if (!game) {
            hosted.stream->end("the game is no longer installed there");
            return;
        }
        if (busy_) {
            hosted.stream->end("a game is already running there");
            return;
        }
        lucent::info("remote", "{} plays {} from here", hosted.guest, game->title);
        if (!hooks_.launch(*game, hosted.audio->environment(), hosted.profile)) {
            hosted.stream->end("the game could not start there");
            return;
        }
        hosted.launched = true;
        hooks_.say(hosted.guest + " plays " + game->title + " from here", false);
        return;
    }
    // A launch that is taken runs, or waits for its saves, from that frame on.
    if (!busy_) {
        hosted.stream->end("the game ended");
    }
}

void RemotePlay::serviceGuest() {
    if (asking_) {
        std::optional<Answer> answer;
        {
            const std::lock_guard lock{*answerMutex_};
            answer = std::exchange(*answer_, std::nullopt);
        }
        if (!answer) {
            return;
        }
        asking_ = false;
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
        lucent::info("remote", "playing {} on {} at {}:{}", game_.title, game_.hostName,
                     answer->offer->stream.address, answer->offer->stream.port);
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
    if (!hosted_) {
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
        glance.state = "asking";
        glance.peer = game_.hostName;
    } else if (guest_) {
        const stream::GuestStream::Timing timing = guest_->timing();
        glance.state = "playing";
        glance.peer = game_.hostName;
        glance.roundTripUs = timing.roundTripUs;
        glance.bytesPerSecond = timing.bytesPerSecond;
        glance.video = video_->summary();
        glance.audio = audio_->summary();
    } else {
        const std::lock_guard lock{hostMutex_};
        if (hosted_) {
            glance.state = "hosting";
            glance.peer = hosted_->guest;
            glance.video = hosted_->stream->linkSummary();
        }
    }
    return glance;
}

} // namespace opensu::app

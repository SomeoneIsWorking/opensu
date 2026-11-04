// remote_play — LAN remote play on the shell's loop, both ways. As host: a paired openSU's request
// to play one of the games here (`offer`, on the LAN listener's thread) opens a `HostStream` with
// the Gamescope this openSU runs in captured into it (`HostVideo`) and the game's sound from a
// sink of the session's own (`HostAudio`), and queues the game, which the loop starts through the
// normal launch path with that sink as its output; the session ends when the game ends, and the
// game is closed when the guest leaves or goes quiet. As guest: Play on another openSU's game asks
// that host (`start`), and once it agrees a `GuestStream` carries the session, its pictures decoded
// by `GuestVideo` for the loop to show and its sound by `GuestAudio` for the shell's device, until
// the player ends it from the Guide menu (`end`) or the host does. One session at a time either
// way.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "audio/live_sound.hpp"
#include "lan/lan_node.hpp"
#include "library/game.hpp"
#include "stream/guest_audio.hpp"
#include "stream/guest_stream.hpp"
#include "stream/guest_video.hpp"
#include "stream/host_audio.hpp"
#include "stream/host_input.hpp"
#include "stream/host_stream.hpp"
#include "stream/host_video.hpp"

namespace opensu::app {

class RemotePlay {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The installed local game with `id`, as the catalog lists it now.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Starts `game` here through the normal launch path, `environment` added to its own; false
        /// when it was refused.
        std::function<bool(const library::Game& game, std::vector<std::string> environment)> launch;
        /// Closes the game started here.
        std::function<void()> close;
        /// Asks host `hostId` to stream its game `gameId` (`LanService::askToPlay`); false when it
        /// cannot be asked now.
        std::function<bool(
            const std::string& hostId, const std::string& gameId,
            std::function<void(std::optional<lan::LanNode::PlayOffer>, std::string)>)>
            ask;
        /// Where a remote game's sound plays: 48 kHz stereo on the shell's device, or none
        /// where the shell plays no sound.
        std::function<std::unique_ptr<audio::LiveSound>()> speaker;
    };

    /// Where a host's game is shown: the Wayland socket of the Gamescope this openSU runs in, whose
    /// screen it streams (empty outside one, where nothing is offered), and that Gamescope's X
    /// display, which the guest's keys and pointer go to.
    struct Screen {
        std::filesystem::path gamescopeSocket;
        std::string x11Display;
    };
    RemotePlay(Screen screen, Hooks hooks);
    ~RemotePlay();
    RemotePlay(const RemotePlay&) = delete;
    RemotePlay& operator=(const RemotePlay&) = delete;
    RemotePlay(RemotePlay&&) = delete;
    RemotePlay& operator=(RemotePlay&&) = delete;

    /// `peer` asks to play `gameId` here, its session sealed with `keys`: the UDP port the stream
    /// takes packets on, or nothing with `refusal`. On the LAN listener's thread.
    [[nodiscard]] std::optional<std::uint16_t> offer(const lan::Peer& peer,
                                                     const std::string& gameId,
                                                     const lan::StreamKeys& keys,
                                                     std::string& refusal);

    /// Plays `game`, another openSU's, from here. Main loop only.
    void start(const library::Game& game);
    /// Ends the session this one plays. Main loop only.
    void end();
    /// Whether this one plays another's game, or is asking to. Main loop only.
    [[nodiscard]] bool playing() const noexcept {
        return guest_ != nullptr || asking_;
    }
    /// Whether another's game streams here now. Main loop only.
    [[nodiscard]] bool streaming() const noexcept {
        return guest_ != nullptr;
    }
    /// The remote game played or asked for. Main loop only.
    [[nodiscard]] const library::Game& game() const noexcept {
        return game_;
    }

    /// Starts a queued game, ends what is over and passes news on; `busy` says whether a game
    /// here runs or is on its way. Every frame, main loop only.
    void service(bool busy);

    /// The player's controls as they stand, for the host's game; sent when they changed. Main
    /// loop only.
    void input(const stream::wire::PadState& pad, const stream::wire::KeyState& keys,
               const stream::wire::MouseState& mouse);
    /// The newest picture from the host not taken yet, to show. Main loop only.
    [[nodiscard]] std::optional<stream::GuestVideo::Frame> takePicture();
    /// The loop submitted the frame showing the host's frame `number` at `submittedUs`; `drawn`
    /// says whether the window had a surface for it. Main loop only.
    void presented(std::uint32_t number, std::uint64_t submittedUs, bool drawn);
    /// The newest remote picture and its number: on a guest the one decoded last, on a host the
    /// one sent last. Main loop only.
    [[nodiscard]] std::optional<stream::GuestVideo::Frame> picture() const;

    /// What the control channel reports.
    struct Glance {
        /// "", "asking", "playing" or "hosting".
        std::string state;
        /// The host's name, or the guest's.
        std::string peer;
        std::uint64_t roundTripUs{0};
        std::uint64_t bytesPerSecond{0};
        /// On a guest, how its frames went (`GuestVideo::summary`).
        std::string video;
        /// On a guest, how its sound came (`GuestAudio::summary`).
        std::string audio;
    };
    [[nodiscard]] Glance glance() const;

  private:
    /// The host's session: the stream, whose it is, and the game it plays.
    struct Hosted {
        /// Before the stream, whose thread plays the guest's input into it.
        std::unique_ptr<stream::HostInput> input;
        std::unique_ptr<stream::HostStream> stream;
        /// After the stream, which they send over.
        std::unique_ptr<stream::HostVideo> video;
        std::unique_ptr<stream::HostAudio> audio;
        std::string guest;
        std::string gameId;
        /// Whether the game was started.
        bool launched{false};
    };
    void serviceHost();
    void serviceGuest();
    /// Ends the guest's session here, the stream first.
    void drop();

    Screen screen_;
    Hooks hooks_;
    /// Whether a game here runs or is on its way, as the loop last saw; read by `offer`.
    std::atomic<bool> busy_{false};
    /// Taken by the listener's thread to offer, by the loop to run it.
    mutable std::mutex hostMutex_;
    std::optional<Hosted> hosted_;

    library::Game game_;
    bool asking_{false};
    /// What the host answered, left by the asking thread for the loop.
    struct Answer {
        std::optional<lan::LanNode::PlayOffer> offer;
        std::string error;
    };
    std::shared_ptr<std::mutex> answerMutex_{std::make_shared<std::mutex>()};
    std::shared_ptr<std::optional<Answer>> answer_{std::make_shared<std::optional<Answer>>()};
    /// Before the stream, whose thread feeds them; the speaker before the sound that plays on it.
    std::unique_ptr<stream::GuestVideo> video_;
    std::unique_ptr<audio::LiveSound> speaker_;
    std::unique_ptr<stream::GuestAudio> audio_;
    std::unique_ptr<stream::GuestStream> guest_;
};

} // namespace opensu::app

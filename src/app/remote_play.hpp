// remote_play — LAN remote play on the shell's loop, both ways. As host: a session streams the game
// here to up to `maxGuests` paired openSUs, the Gamescope this openSU runs in captured once
// (`HostVideo`) and the game's sound once (`HostAudio`: a sink of the session's own for a game
// started for a guest, this computer's output for the player's own game), both sent to every
// guest of its `Audience`, each guest with a `HostStream` and a pad (`HostInput`) of its own. A
// paired openSU's request to play (`offer`, on the LAN listener's thread) starts the game for it
// through the normal launch path when nothing runs here; while the same game runs here, started by
// the player or for another guest, the guest asks to join, the player is asked (`joinQuestion`,
// `answerJoin`) and the guest is let in when it next asks how its ask stands (`playStatus`). A
// guest leaving frees its pad and the game goes on; the game ending ends every guest's stream, and
// a game started for a guest closes when its last guest leaves. As guest: Play on another openSU's
// game asks that host (`start`), waiting while its player is asked when the game runs there, and
// once it agrees a `GuestStream` carries the session, its pictures decoded by `GuestVideo` for the
// loop to show and its sound by `GuestAudio` for the shell's device, until the player ends it from
// the Guide menu (`end`) or the host does. One session at a time either way. The player here
// invites a paired openSU to the game that runs here (`invite`), which lets it in without asking
// for 120 s; that openSU's player is asked (`invited`, `invitation`, `answerInvitation`) and Join
// plays the game as above. `kick` ends one guest's stream.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "audio/live_sound.hpp"
#include "lan/lan_listener.hpp"
#include "lan/lan_node.hpp"
#include "library/game.hpp"
#include "stream/audience.hpp"
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
    /// The most guests one game here takes.
    static constexpr std::size_t maxGuests = 4;
    /// How long a guest's ask to join waits for the player.
    static constexpr std::chrono::seconds askPatience{120};

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The installed local game with `id`, as the catalog lists it now.
        std::function<std::optional<library::Game>(const std::string& id)> find;
        /// Starts `game` here through the normal launch path as `profile`, `environment` added to
        /// its own; false when it was refused.
        std::function<bool(const library::Game& game, std::vector<std::string> environment,
                           const std::string& profile)>
            launch;
        /// Closes the game started here.
        std::function<void()> close;
        /// Asks host `hostId` to stream its game `gameId` (`LanService::askToPlay`), `waiting`
        /// called when its player is asked; false when it cannot be asked now.
        std::function<bool(
            const std::string& hostId, const std::string& gameId, std::function<void()> waiting,
            std::function<void(std::optional<lan::LanNode::PlayOffer>, std::string)>)>
            ask;
        /// Where a remote game's sound plays: 48 kHz stereo on the shell's device, or none
        /// where the shell plays no sound.
        std::function<std::unique_ptr<audio::LiveSound>()> speaker;
        /// Tells the paired openSU `peerId` of `game`, `refused` called with why off the loop when
        /// it will not be asked; false when it cannot be told now.
        std::function<bool(const std::string& peerId, const lan::wire::Invitation& game,
                           std::function<void(std::string)> refused)>
            invite;
        /// Plays host `hostId`'s game `gameId` from here; false when the catalog does not list it.
        std::function<bool(const std::string& hostId, const std::string& gameId)> join;
        /// The name this openSU goes by.
        std::function<std::string()> name;
    };

    /// Where a host's game is shown: the Wayland socket of the Gamescope this openSU runs in, whose
    /// screen it streams (empty outside one, where nothing is offered), and that Gamescope's X
    /// display, which the keys and pointer of the guest a game was started for go to.
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

    /// `peer` asks to play `gameId` here as `profile`, its session sealed with `keys`: the UDP
    /// port the stream takes packets on, waiting while the player is asked to let it join the game
    /// that runs here, or a refusal. On the LAN listener's thread.
    [[nodiscard]] lan::PlayOutcome offer(const lan::Peer& peer, const std::string& gameId,
                                         const std::string& profile, const lan::StreamKeys& keys);
    /// How `peer`'s ask to join `gameId` stands; let in now, with the port, when the player allowed
    /// it. Nothing when it has no ask. On the LAN listener's thread.
    [[nodiscard]] std::optional<lan::wire::PlayVerdict> playStatus(const lan::Peer& peer,
                                                                   const std::string& gameId);

    /// A guest's ask to join the game here that waits for the player.
    struct JoinQuestion {
        lan::Peer peer;
        std::string title;

        bool operator==(const JoinQuestion&) const = default;
    };
    /// The oldest ask that waits. Main loop only.
    [[nodiscard]] std::optional<JoinQuestion> joinQuestion() const;
    /// The player's answer to `peerId`'s ask. Main loop only.
    void answerJoin(const std::string& peerId, bool allow);

    /// Whether the game that runs here can take one more guest. Main loop only.
    [[nodiscard]] bool takesGuests() const;
    /// The guests of the game here, in join order. Main loop only.
    [[nodiscard]] std::vector<lan::Peer> guests() const;
    /// `peers` without the guests of the game here. Main loop only.
    [[nodiscard]] std::vector<lan::Peer> notGuests(std::vector<lan::Peer> peers) const;
    /// Whether the one guest there is had the game started for it, so it closes as it goes. Main
    /// loop only.
    [[nodiscard]] bool closesWithGuest() const;
    /// Invites `peer` to the game that runs here. Main loop only.
    void invite(const lan::Peer& peer);
    /// Ends `peerId`'s stream; it is told this openSU's player removed it. Main loop only.
    void kick(const std::string& peerId);

    /// `peer` invites this one to its game: why that will not be put to the player, or nothing.
    /// On the LAN listener's thread.
    [[nodiscard]] std::optional<std::string> invited(const lan::Peer& peer,
                                                     const lan::wire::Invitation& game);
    /// An invitation that waits for the player here.
    struct Invited {
        lan::Peer peer;
        std::string gameId;
        std::string title;
        std::chrono::steady_clock::time_point at;

        bool operator==(const Invited&) const = default;
    };
    /// The oldest invitation that waits. Main loop only.
    [[nodiscard]] std::optional<Invited> invitation() const;
    /// The player's answer to `hostId`'s invitation; Join plays its game. Main loop only.
    void answerInvitation(const std::string& hostId, bool join);

    /// Plays `game`, another openSU's, from here, the pad with `mapping` (this openSU's for the
    /// game), its pictures decoded into `pictures`. Main loop only.
    void start(const library::Game& game, stream::wire::PadMapping mapping,
               stream::PictureMemory pictures);
    /// The screen could not sample a DMA-BUF picture: the rest of the session's are read back.
    void readPicturesBack();
    /// Ends the session this one plays. Main loop only.
    void end();
    /// Whether this one plays another's game, or is asking to. Main loop only.
    [[nodiscard]] bool playing() const noexcept {
        return guest_ != nullptr || asking_;
    }
    /// Whether this one waits for the host's player to let it in. Main loop only.
    [[nodiscard]] bool waiting() const noexcept {
        return asking_ && waiting_->load();
    }
    /// Whether another's game streams here now. Main loop only.
    [[nodiscard]] bool streaming() const noexcept {
        return guest_ != nullptr;
    }
    /// Whether the game streaming here was running there already, so it goes on when this one
    /// leaves. Main loop only.
    [[nodiscard]] bool joined() const noexcept {
        return joined_;
    }
    /// The remote game played or asked for. Main loop only.
    [[nodiscard]] const library::Game& game() const noexcept {
        return game_;
    }

    /// Starts a queued game, ends what is over, lets go of guests who left and passes news on;
    /// `busy` says whether a game here runs or is on its way, `running` is the one that runs.
    /// Every frame, main loop only.
    void service(bool busy, const library::Game* running);

    /// The player's controls as they stand, for the host's game; sent when they changed. Main
    /// loop only.
    void input(const stream::wire::PadState& pad, const stream::wire::KeyState& keys,
               const stream::wire::MouseState& mouse);
    /// A test run's slow link: takes at most `bitsPerSecond` from the host from now on, 0 for
    /// all. Main loop only.
    void capLink(std::uint64_t bitsPerSecond);
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
        /// "", "asking", "waiting" (the host's player is asked), "playing" or "hosting".
        std::string state;
        /// The host's name, or the guests' in join order.
        std::string peer;
        std::uint64_t roundTripUs{0};
        std::uint64_t bytesPerSecond{0};
        /// On a guest, how its frames went (`GuestVideo::summary`); on a host, each guest's link,
        /// its plan and measures (`HostStream::linkSummary`), after its name.
        std::string video;
        /// On a guest, how its sound came (`GuestAudio::summary`).
        std::string audio;
        /// On a host, how many guests play and whose ask to join waits for the player.
        std::size_t guests{0};
        std::string asking;
    };
    [[nodiscard]] Glance glance() const;

  private:
    /// One guest of the host's session: who, its pad and the stream it plays over.
    struct Guest {
        lan::Peer peer;
        /// Before the stream, whose thread plays the guest's input into it.
        std::unique_ptr<stream::HostInput> input;
        std::unique_ptr<stream::HostStream> stream;
        /// The player here removed it, and was told so.
        bool kicked{false};
    };
    /// The host's session: the guests in join order and what is sent to them.
    struct Hosted {
        /// Before what sends to it.
        stream::Audience audience;
        std::vector<std::unique_ptr<Guest>> guests;
        std::unique_ptr<stream::HostVideo> video;
        std::unique_ptr<stream::HostAudio> audio;
        std::string gameId;
        std::string title;
        /// The profile a game started for a guest plays as here.
        std::string profile;
        /// Started for its first guest, rather than by the player here.
        bool forGuest{false};
        /// Whether the game was started.
        bool launched{false};
    };
    /// A guest's ask to join, until it hears the answer.
    struct Ask {
        enum class Verdict : std::uint8_t { Waiting, Allowed, Declined };
        lan::Peer peer;
        std::string gameId;
        lan::StreamKeys keys;
        std::chrono::steady_clock::time_point at;
        Verdict verdict{Verdict::Waiting};
        std::string why;
    };
    /// The player here invited a peer: its ask to join this game is let in without a question.
    struct Sent {
        std::string peerId;
        std::string gameId;
        std::chrono::steady_clock::time_point at;
    };

    /// Makes `peer` a guest of `hosted` over a stream sealed with `keys`, the keys and pointer
    /// through X when `withKeys`; its UDP port. Throws `std::runtime_error` when it cannot.
    /// Holding `hostMutex_`.
    std::uint16_t admit(Hosted& hosted, const lan::Peer& peer, const lan::StreamKeys& keys,
                        bool withKeys);
    /// The player's own game, streamed: its picture and this computer's output. Throws
    /// `std::runtime_error` when it cannot be captured. Holding `hostMutex_`.
    void hostOwnGame(const Ask& ask);
    /// `peer` waits for the player; its ask is kept with `keys`. Holding `hostMutex_`.
    lan::PlayOutcome waitFor(const lan::Peer& peer, const std::string& gameId,
                             const lan::StreamKeys& keys);
    /// Asks that waited too long are declined, and answers no guest came for are dropped. Holding
    /// `hostMutex_`.
    void lapseAsks();
    void serviceHost(const library::Game* running);
    void serviceGuest();
    /// Ends the guest's session here, the stream first.
    void drop();

    Screen screen_;
    Hooks hooks_;
    /// Whether a game here runs or is on its way, as the loop last saw; read by `offer`.
    std::atomic<bool> busy_{false};
    /// Taken by the listener's thread to offer and let in, by the loop to run the session.
    mutable std::mutex hostMutex_;
    std::unique_ptr<Hosted> hosted_;
    std::vector<Ask> asks_;
    std::vector<Sent> sent_;
    std::vector<Invited> received_;
    /// Whether a game plays here or from another, as the loop last saw; read by `invited`.
    std::atomic<bool> occupied_{false};
    /// The game that runs here as the loop last saw, its id and title.
    std::string runningId_;
    std::string runningTitle_;

    library::Game game_;
    stream::wire::PadMapping mapping_;
    /// The cap a test run set, kept for the next session.
    std::uint64_t linkCap_{0};
    bool asking_{false};
    bool joined_{false};
    /// Whether the host's player is asked now, set by the asking thread; and whether it was said.
    std::shared_ptr<std::atomic<bool>> waiting_{std::make_shared<std::atomic<bool>>(false)};
    bool saidWaiting_{false};
    /// What the host answered, left by the asking thread for the loop.
    struct Answer {
        std::optional<lan::LanNode::PlayOffer> offer;
        std::string error;
    };
    std::shared_ptr<std::mutex> answerMutex_{std::make_shared<std::mutex>()};
    std::shared_ptr<std::optional<Answer>> answer_{std::make_shared<std::optional<Answer>>()};
    /// Invitations refused, said on the loop; under `answerMutex_`.
    std::shared_ptr<std::vector<std::string>> refusals_{
        std::make_shared<std::vector<std::string>>()};
    /// Before the stream, whose thread feeds them; the speaker before the sound that plays on it.
    std::unique_ptr<stream::GuestVideo> video_;
    stream::PictureMemory pictures_{stream::PictureMemory::Cpu};
    std::unique_ptr<audio::LiveSound> speaker_;
    std::unique_ptr<stream::GuestAudio> audio_;
    std::unique_ptr<stream::GuestStream> guest_;
};

} // namespace opensu::app

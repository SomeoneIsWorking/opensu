// session — one netplay game, whichever core plays it. Every machine runs the same game from the
// same files and must see the same controls on the same frame: a player's input for frame `f` is
// sent while `f - delay` runs, each message repeating the frames before it so a lost one costs
// nothing, and a frame runs only once every player's input for it is in, unless `ahead` lets this
// machine run on a repeat of a player's last input: when the real input then differs, `takeWrong`
// names the first frame to run again and `rewind` goes back to it. The machines compare a checksum
// of the game's state now and then, on frames no input can change any more, and the session ends
// when they differ, when another player says so or when nothing is heard for too long. What a core
// does with the input is its own; the session knows ports and frames, not games. The transport is
// `transport`'s.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "transport.hpp"

namespace opensu::netplay {

/// One player's controls on one frame: the libretro joypad mask and the two sticks.
struct PlayerInput {
    std::uint16_t buttons{0};
    /// Left x and y, right x and y.
    std::array<std::int16_t, 4> sticks{};

    bool operator==(const PlayerInput&) const = default;
};

struct SessionConfig {
    /// How many players, each a port and a slot in this session.
    unsigned players{2};
    /// The port this machine plays.
    unsigned slot{0};
    /// How many frames after it is read a machine's input takes effect, on every machine.
    unsigned delay{2};
    /// How many frames a machine may run past the input it has; 0 is lockstep.
    unsigned ahead{0};
    /// How long the others may stay silent once heard, and before they are first heard.
    std::chrono::milliseconds quiet{std::chrono::seconds{5}};
    std::chrono::milliseconds joining{std::chrono::seconds{20}};
};

/// Why a session is over.
enum class SessionEnd : std::uint8_t {
    /// It is not.
    None,
    /// Another player left.
    Left,
    /// Nothing was heard from the others for too long.
    Quiet,
    /// The machines' games differ.
    Desync,
};

class Session {
  public:
    using Clock = std::chrono::steady_clock;

    /// How many frames apart the machines compare their games' state.
    static constexpr std::uint64_t checkInterval = 120;
    /// How many frames each input message repeats.
    static constexpr std::size_t repeated = 16;

    Session(Transport& transport, SessionConfig config);

    /// This machine's input as read now, taking effect `delay` frames after the current one, sent
    /// at once. Once per frame, before `ready` asks for that frame.
    void submit(const PlayerInput& local);
    /// Whether every player's input for the current frame is in, taking what arrived and waiting
    /// at most `wait`. While it is not, the last input is sent again.
    [[nodiscard]] bool ready(std::chrono::milliseconds wait);
    /// `slot`'s input for the current frame, once `ready`: a repeat of its last one when not in.
    [[nodiscard]] const PlayerInput& input(unsigned slot) const;
    /// The current frame ran, on what `input` gave.
    void advance();
    /// Whether the state of the current frame is one to compare, so the caller works out `check`.
    [[nodiscard]] bool checkDue() const noexcept;
    /// This machine's checksum of the game's state after `frame`, which must be below `confirmed`.
    void check(std::uint64_t frame, std::uint32_t checksum);
    /// Frames below this have every player's real input.
    [[nodiscard]] std::uint64_t confirmed() const;
    /// The first frame that ran on a wrong guess, once; the caller goes back to it.
    [[nodiscard]] std::optional<std::uint64_t> takeWrong();
    /// The next frame is `frame` again; `advance` catches up.
    void rewind(std::uint64_t frame) noexcept;
    /// Runs lockstep, with `ahead` frames of delay instead. Before the first frame.
    void lockstep() noexcept;
    /// The clock on the others' arrival starts again, after what came before the first frame.
    void restartJoining() noexcept;
    /// The session is over for `why`, found outside it.
    void end(SessionEnd why) noexcept;
    /// Tells the others this machine is going.
    void leave();

    [[nodiscard]] std::uint64_t frame() const noexcept {
        return frame_;
    }
    [[nodiscard]] unsigned slot() const noexcept {
        return config_.slot;
    }
    [[nodiscard]] SessionEnd ended() const noexcept {
        return ended_;
    }
    /// How many checksums matched the other machine's.
    [[nodiscard]] std::uint64_t matched() const noexcept {
        return matched_;
    }
    /// How often `ready` found the frame's input missing.
    [[nodiscard]] std::uint64_t stalls() const noexcept {
        return stalls_;
    }

  private:
    static constexpr std::size_t ring = 256;

    struct Player {
        std::array<PlayerInput, ring> frames{};
        /// What each frame ran on, to compare with what comes.
        std::array<PlayerInput, ring> used{};
        /// The first frame whose input has not come; the first `delay` frames are neutral.
        std::uint64_t known{0};
    };

    void sendInput();
    void take(std::string_view message);
    void takeInput(std::string_view body);
    void takeCheck(std::string_view body);
    void compare(std::uint64_t frame);
    [[nodiscard]] bool canRun() const;

    Transport& transport_;
    SessionConfig config_;
    std::vector<Player> players_;
    std::uint64_t frame_{0};
    std::map<std::uint64_t, std::uint32_t> ownChecks_;
    std::map<std::uint64_t, std::uint32_t> otherChecks_;
    Clock::time_point started_{Clock::now()};
    std::optional<Clock::time_point> heard_;
    Clock::time_point sent_{};
    SessionEnd ended_{SessionEnd::None};
    std::optional<std::uint64_t> wrong_;
    bool stalled_{false};
    std::uint64_t stalls_{0};
    std::uint64_t matched_{0};
};

} // namespace opensu::netplay

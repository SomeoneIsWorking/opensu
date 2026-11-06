// state_sync — the machines of a rollback session starting from one state. Player 1 offers the
// size and SHA-256 of its state at frame 0. A machine that already has that state takes nothing, so
// on a core that boots alike no peer's bytes come near it. Otherwise the size must be this core's
// own before anything is allocated, and the whole must hash to what was offered before the caller
// gets it to load. Nothing else about a peer's state can be checked, so the code's trust is in the
// friend it was sent to.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "transport.hpp"

namespace opensu::netplay {

class StateSync {
  public:
    using Clock = std::chrono::steady_clock;

    enum class Role : std::uint8_t { Offers, Takes };
    enum class Outcome : std::uint8_t {
        /// Not done.
        Running,
        /// The machines start from one state, and this one has nothing to load.
        Same,
        /// `state` holds the checked state to load.
        Received,
        /// The other machine's state failed a check.
        Refused,
        /// Nothing came for too long.
        Lapsed,
    };

    static constexpr std::size_t chunkBytes = 1000;
    static constexpr std::chrono::seconds patience{30};

    /// `own` is this machine's state, which must outlive this.
    StateSync(Transport& transport, Role role, std::span<const std::byte> own);

    /// Sends what is due and takes what came, waiting at most `wait`.
    Outcome pump(std::chrono::milliseconds wait);
    /// The state taken, once `Received`.
    [[nodiscard]] std::span<const std::byte> state() const noexcept {
        return received_;
    }

  private:
    void take(std::string_view message);
    void takeOffer(std::string_view message);
    void takeChunk(std::string_view message);
    void sendDue();
    void sendWindow();
    void ask();
    void finish(Outcome outcome);

    Transport& transport_;
    Role role_;
    std::span<const std::byte> own_;
    std::array<std::uint8_t, 32> hash_{};
    std::vector<std::byte> received_;
    bool offered_{false};
    /// The first chunk the other machine lacks.
    std::uint32_t next_{0};
    Outcome outcome_{Outcome::Running};
    Clock::time_point heard_{Clock::now()};
    Clock::time_point sent_{};
};

} // namespace opensu::netplay

// netplay_link — this runner as one machine of a netplay session: the session the shell described
// in a file (`--netplay`), this machine's pad into it each frame, what the core reads taken from
// it, and every so often a checksum of the core's memory for the machines to compare. A frame runs
// only when every player's input for it is in, unless the session asks for rollback and the core
// can: then both machines start from one state (`netplay::StateSync`), a state is kept per frame,
// and a frame that ran on a wrong guess is run again from its state with the sound and picture off.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core_environment.hpp"
#include "joypad.hpp"
#include "netplay/session.hpp"
#include "netplay/state_sync.hpp"
#include "netplay/udp_transport.hpp"
#include "port_input.hpp"

namespace opensu::retro {

class Core;

/// The session a `--netplay` file describes.
struct NetplaySetup {
    netplay::SessionConfig session;
    /// The UDP port to bind, 0 for any.
    std::uint16_t port{0};
    /// The other machine; nothing to learn it from what it sends.
    std::optional<stream::UdpAddress> peer;
    std::vector<std::uint8_t> sendKey;
    std::vector<std::uint8_t> receiveKey;
    NetplayClock clock;
};

/// The JSON `text`: `{"slot": 0, "players": 2, "delay": 2, "ahead": 6, "port": 0,
/// "peer": "192.168.1.2:4000", "clockMs": 1793900000000, "utcOffset": 10800, "sendKey": hex,
/// "receiveKey": hex}`, `peer` and `ahead` optional. Nothing, with `error`, when malformed.
[[nodiscard]] std::optional<NetplaySetup> netplaySetupOf(const std::string& text,
                                                         std::string& error);

class NetplayLink final : public PortInput {
  public:
    NetplayLink(const NetplaySetup& setup, const Joypad& joypad);

    /// `core`, loaded: settles rollback or lockstep, which every machine works out alike.
    void attach(Core& core, const CoreEnvironment& environment);
    /// Whether the next frame may run: reads this machine's pad once per frame into the session,
    /// waits at most `wait` for the others', and puts right what ran on a wrong guess.
    [[nodiscard]] bool ready(Core& core, std::chrono::milliseconds wait);
    /// The frame ran on `core`.
    void ran(Core& core);
    /// Tells the others this machine is going, unless the session is over already.
    void leave();

    [[nodiscard]] netplay::SessionEnd ended() const noexcept {
        return session_.ended();
    }
    [[nodiscard]] unsigned slot() const noexcept {
        return session_.slot();
    }
    [[nodiscard]] std::uint64_t stalls() const noexcept {
        return session_.stalls();
    }
    [[nodiscard]] std::uint64_t matched() const noexcept {
        return session_.matched();
    }
    [[nodiscard]] std::uint16_t port() const noexcept {
        return transport_.port();
    }
    [[nodiscard]] bool rollingBack() const noexcept {
        return !states_.empty();
    }
    /// How many frames were run again.
    [[nodiscard]] std::uint64_t replayed() const noexcept {
        return replayed_;
    }

    [[nodiscard]] std::int16_t state(unsigned port, unsigned id) const override;
    [[nodiscard]] std::int16_t analog(unsigned port, unsigned index, unsigned id) const override;

  private:
    [[nodiscard]] unsigned playerCount() const noexcept {
        return players_;
    }

    /// Whether the machines are past their first state; ends the session when one fails a check.
    [[nodiscard]] bool started(Core& core, std::chrono::milliseconds wait);
    void keepState(Core& core);
    void rollBack(Core& core, std::uint64_t frame);
    void finishFrame(Core& core);
    void sendChecks();

    unsigned players_;
    unsigned ahead_;
    netplay::UdpTransport transport_;
    netplay::Session session_;
    const Joypad& joypad_;
    bool submitted_{false};
    /// The state before each of the last frames, by frame number around the ring.
    std::vector<std::vector<std::byte>> states_;
    std::vector<std::byte> firstState_;
    std::unique_ptr<netplay::StateSync> sync_;
    /// Checksums of frames whose input may still change.
    std::map<std::uint64_t, std::uint32_t> checks_;
    std::uint64_t replayed_{0};
};

} // namespace opensu::retro

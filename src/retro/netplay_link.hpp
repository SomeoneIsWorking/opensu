// netplay_link — this runner as one machine of a netplay session: the session the shell described
// in a file (`--netplay`), this machine's pad into it each frame, what the core reads taken from
// it, so a frame runs only when every player's input for it is in, and every so often a checksum
// of the core's memory for the machines to compare.
#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "core_environment.hpp"
#include "joypad.hpp"
#include "netplay/session.hpp"
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

/// The JSON `text`: `{"slot": 0, "players": 2, "delay": 2, "port": 0, "peer": "192.168.1.2:4000",
/// "clockMs": 1793900000000, "utcOffset": 10800, "sendKey": hex, "receiveKey": hex}`, `peer`
/// optional. Nothing, with `error`, when malformed.
[[nodiscard]] std::optional<NetplaySetup> netplaySetupOf(const std::string& text,
                                                         std::string& error);

class NetplayLink final : public PortInput {
  public:
    NetplayLink(const NetplaySetup& setup, const Joypad& joypad);

    /// Whether the next frame may run: reads this machine's pad once per frame into the session
    /// and waits at most `wait` for the others'.
    [[nodiscard]] bool ready(std::chrono::milliseconds wait);
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

    [[nodiscard]] std::int16_t state(unsigned port, unsigned id) const override;
    [[nodiscard]] std::int16_t analog(unsigned port, unsigned index, unsigned id) const override;

  private:
    [[nodiscard]] unsigned playerCount() const noexcept {
        return players_;
    }

    unsigned players_;
    netplay::UdpTransport transport_;
    netplay::Session session_;
    const Joypad& joypad_;
    bool submitted_{false};
};

} // namespace opensu::retro

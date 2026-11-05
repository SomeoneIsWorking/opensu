// transport — how a netplay session's messages reach the other players. A session only sends and
// takes datagrams, so a path is one implementation of this: the LAN's sealed UDP
// (`lan_transport`), and later an internet one found through an invite code.
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace opensu::netplay {

class Transport {
  public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(Transport&&) = delete;
    virtual ~Transport() = default;

    /// Sends `message` to every other player; it may be lost.
    virtual void send(std::string_view message) = 0;
    /// The next message from any other player, waiting at most `wait`; nothing when none came.
    [[nodiscard]] virtual std::optional<std::string> receive(std::chrono::milliseconds wait) = 0;
};

} // namespace opensu::netplay

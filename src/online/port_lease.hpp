// port_lease — a router's mapping held for as long as this object lives: renewed at half its lease
// on a thread of its own, taken down when it goes. Should openSU die instead, the mapping lapses
// with its short lease.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "port_mapper.hpp"

namespace opensu::online {

/// What a mapping is asked for, and renewed at half of.
inline constexpr std::chrono::seconds askedLease{120};

class PortLease {
  public:
    /// Keeps `mapping`, which `mapper` made for local `port`.
    PortLease(std::unique_ptr<PortMapper> mapper, std::uint16_t port, Mapping mapping);
    ~PortLease();
    PortLease(const PortLease&) = delete;
    PortLease& operator=(const PortLease&) = delete;
    PortLease(PortLease&&) = delete;
    PortLease& operator=(PortLease&&) = delete;

    [[nodiscard]] const stream::UdpAddress& external() const noexcept {
        return external_;
    }

  private:
    void keep(const std::stop_token& stop, std::chrono::seconds lease);

    std::unique_ptr<PortMapper> mapper_;
    std::uint16_t port_;
    stream::UdpAddress external_;
    std::jthread keeper_;
};

} // namespace opensu::online

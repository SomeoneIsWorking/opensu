// sd_bus — owning handles over sd-bus's bus, message and slot references, its error, and how long
// a poll may wait for the bus's next timeout. Shared by everything openSU puts on D-Bus.
#pragma once

#include <memory>
#include <string>

#include <systemd/sd-bus.h>

namespace opensu::host::sdbus {

struct BusClose {
    void operator()(sd_bus* bus) const noexcept {
        sd_bus_flush_close_unref(bus);
    }
};
using Bus = std::unique_ptr<sd_bus, BusClose>;

struct MessageUnref {
    void operator()(sd_bus_message* message) const noexcept {
        sd_bus_message_unref(message);
    }
};
using Message = std::unique_ptr<sd_bus_message, MessageUnref>;

struct SlotUnref {
    void operator()(sd_bus_slot* slot) const noexcept {
        sd_bus_slot_unref(slot);
    }
};
using Slot = std::unique_ptr<sd_bus_slot, SlotUnref>;

/// An sd-bus error, freed when it goes.
class BusError {
  public:
    BusError() = default;
    ~BusError() {
        sd_bus_error_free(&error_);
    }
    BusError(const BusError&) = delete;
    BusError& operator=(const BusError&) = delete;
    BusError(BusError&&) = delete;
    BusError& operator=(BusError&&) = delete;

    [[nodiscard]] sd_bus_error* get() noexcept {
        return &error_;
    }
    /// The bus's message, else what `code` (a negative errno) means.
    [[nodiscard]] std::string text(int code) const;

  private:
    sd_bus_error error_{};
};

/// How long `poll` may wait for `bus`'s next timeout, in milliseconds; -1 for none.
[[nodiscard]] int pollTimeout(sd_bus* bus);

} // namespace opensu::host::sdbus

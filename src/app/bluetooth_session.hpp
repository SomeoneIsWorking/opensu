// bluetooth_session — the Bluetooth state the shell shows and the changes it asks for, off the
// loop. BlueZ answers a read in milliseconds but a pairing takes up to half a minute, so reads and
// changes run on a worker, one at a time, and the loop takes what they found. The Devices page and
// the quick menu share one session, so they never disagree.
#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <string>

#include "host/bluetooth.hpp"

namespace opensu::app {

/// A change that finished, for the player to be told.
struct BluetoothNotice {
    std::string text;
    bool error{false};
};

class BluetoothSession {
  public:
    using Clock = std::chrono::steady_clock;

    /// How often BlueZ is read while something shows its state, and while a scan runs.
    static constexpr std::chrono::milliseconds readInterval{2000};
    static constexpr std::chrono::milliseconds scanInterval{1000};

    /// `backend` outlives the session.
    explicit BluetoothSession(host::Bluetooth& backend) noexcept : backend_{&backend} {
    }
    ~BluetoothSession();
    BluetoothSession(const BluetoothSession&) = delete;
    BluetoothSession& operator=(const BluetoothSession&) = delete;

    /// What BlueZ held at the last read.
    [[nodiscard]] const host::BluetoothState& state() const noexcept {
        return state_;
    }
    /// Whether a read has come back yet.
    [[nodiscard]] bool known() const noexcept {
        return known_;
    }
    /// What the change in progress is doing ("Pairing Joy-Con (R)"), or empty when none is.
    [[nodiscard]] const std::string& busyWith() const noexcept {
        return busyWith_;
    }
    [[nodiscard]] bool scanning() const noexcept {
        return state_.adapter && state_.adapter->discovering;
    }

    /// Takes the finished read or change and starts the next read when `wanted` and one is due.
    /// Returns whether anything shown changed.
    bool poll(Clock::time_point now, bool wanted);
    /// Reads at the next poll, whatever the interval.
    void readSoon() noexcept {
        due_ = true;
    }
    /// The finished change's outcome, once.
    [[nodiscard]] std::optional<BluetoothNotice> takeNotice();
    /// The battery BlueZ reports for the device with `address` (any case), if it does.
    [[nodiscard]] std::optional<int> batteryOf(const std::string& address) const;

    // Changes. Each starts one on the worker; nothing starts while another is in progress, and
    // the refusal says so.
    std::string setPowered(bool powered);
    /// Starts a scan, or ends the one that runs.
    std::string toggleScan();
    /// Ends a scan if one runs; the page and the menu leave with it.
    void stopScan();
    /// Pairs with the device, trusts it and connects.
    std::string pair(const host::BluetoothDevice& device);
    std::string connect(const host::BluetoothDevice& device);
    std::string disconnect(const host::BluetoothDevice& device);
    std::string forget(const host::BluetoothDevice& device);

  private:
    struct Outcome {
        BluetoothNotice notice;
        host::BluetoothState state;
    };

    /// Runs `work` on the worker, which gives the notice; one at a time.
    template <class Work> std::string start(std::string busy, Work work);

    host::Bluetooth* backend_;
    host::BluetoothState state_;
    bool known_{false};
    bool due_{true};
    Clock::time_point lastRead_{};
    std::string busyWith_;
    std::future<host::BluetoothState> reading_;
    std::future<Outcome> changing_;
    std::optional<BluetoothNotice> notice_;
};

} // namespace opensu::app

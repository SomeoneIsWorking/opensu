#include "bluetooth_session.hpp"

#include <utility>
#include <algorithm>
#include <cctype>

namespace opensu::app {
namespace {

bool ready(const auto& future) {
    return future.valid() && future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

BluetoothSession::~BluetoothSession() {
    if (reading_.valid()) {
        reading_.wait();
    }
    if (changing_.valid()) {
        changing_.wait();
    }
}

bool BluetoothSession::poll(Clock::time_point now, bool wanted) {
    bool changed = false;
    if (ready(changing_)) {
        Outcome done = changing_.get();
        notice_ = std::move(done.notice);
        busyWith_.clear();
        state_ = std::move(done.state);
        known_ = true;
        lastRead_ = now;
        changed = true;
    }
    if (ready(reading_)) {
        host::BluetoothState next = reading_.get();
        changed = changed || !known_ || next != state_;
        state_ = std::move(next);
        known_ = true;
    }
    const auto interval = scanning() ? scanInterval : readInterval;
    if (wanted && !reading_.valid() && !changing_.valid() && (due_ || now - lastRead_ >= interval)) {
        due_ = false;
        lastRead_ = now;
        reading_ = std::async(std::launch::async, [backend = backend_] {
            return backend->read();
        });
    }
    return changed;
}

std::optional<BluetoothNotice> BluetoothSession::takeNotice() {
    return std::exchange(notice_, std::nullopt);
}

std::optional<int> BluetoothSession::batteryOf(const std::string& address) const {
    const std::string wanted = lowered(address);
    for (const host::BluetoothDevice& device : state_.devices) {
        if (device.battery && lowered(device.address) == wanted) {
            return device.battery;
        }
    }
    return std::nullopt;
}

template <class Work> std::string BluetoothSession::start(std::string busy, Work work) {
    if (changing_.valid()) {
        return busyWith_ + " is not finished yet";
    }
    if (reading_.valid()) {
        // A read in flight is about to be stale; wait for it, it takes milliseconds.
        reading_.wait();
        reading_ = {};
    }
    busyWith_ = std::move(busy);
    changing_ = std::async(std::launch::async, [backend = backend_, work = std::move(work)] {
        Outcome outcome;
        outcome.notice = work(*backend);
        outcome.state = backend->read();
        return outcome;
    });
    return {};
}

std::string BluetoothSession::setPowered(bool powered) {
    return start(powered ? "Turning Bluetooth on" : "Turning Bluetooth off",
                 [powered](host::Bluetooth& backend) {
                     const std::string refused = backend.setPowered(powered);
                     return refused.empty() ? BluetoothNotice{powered ? "Bluetooth is on"
                                                                      : "Bluetooth is off",
                                                              false}
                                            : BluetoothNotice{refused, true};
                 });
}

std::string BluetoothSession::toggleScan() {
    const bool stopping = scanning();
    return start(stopping ? "Stopping the scan" : "Starting a scan",
                 [stopping](host::Bluetooth& backend) {
                     const std::string refused =
                         stopping ? backend.stopDiscovery() : backend.startDiscovery();
                     return refused.empty() ? BluetoothNotice{} : BluetoothNotice{refused, true};
                 });
}

void BluetoothSession::stopScan() {
    if (scanning()) {
        toggleScan();
    }
}

std::string BluetoothSession::pair(const host::BluetoothDevice& device) {
    return start("Pairing " + device.name, [device](host::Bluetooth& backend) {
        if (const std::string refused = backend.pair(device.path); !refused.empty()) {
            return BluetoothNotice{device.name + ": " + refused, true};
        }
        // A trusted device reconnects on its own; either step failing leaves it paired.
        std::string trouble;
        if (const std::string refused = backend.setTrusted(device.path, true); !refused.empty()) {
            trouble = "could not trust it: " + refused;
        }
        if (const std::string refused = backend.connect(device.path); !refused.empty()) {
            trouble += (trouble.empty() ? "" : "; ") + std::string{"could not connect: "} + refused;
        }
        return trouble.empty() ? BluetoothNotice{"Paired and connected " + device.name, false}
                               : BluetoothNotice{"Paired " + device.name + "; " + trouble, true};
    });
}

std::string BluetoothSession::connect(const host::BluetoothDevice& device) {
    return start("Connecting " + device.name, [device](host::Bluetooth& backend) {
        const std::string refused = backend.connect(device.path);
        return refused.empty() ? BluetoothNotice{"Connected " + device.name, false}
                               : BluetoothNotice{device.name + ": " + refused, true};
    });
}

std::string BluetoothSession::disconnect(const host::BluetoothDevice& device) {
    return start("Disconnecting " + device.name, [device](host::Bluetooth& backend) {
        const std::string refused = backend.disconnect(device.path);
        return refused.empty() ? BluetoothNotice{"Disconnected " + device.name, false}
                               : BluetoothNotice{device.name + ": " + refused, true};
    });
}

std::string BluetoothSession::forget(const host::BluetoothDevice& device) {
    return start("Forgetting " + device.name, [device](host::Bluetooth& backend) {
        const std::string refused = backend.remove(device.path);
        return refused.empty() ? BluetoothNotice{"Forgot " + device.name, false}
                               : BluetoothNotice{device.name + ": " + refused, true};
    });
}

} // namespace opensu::app

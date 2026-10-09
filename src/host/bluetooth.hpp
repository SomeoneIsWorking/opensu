// bluetooth — the Bluetooth adapter and its devices as BlueZ reports and changes them over D-Bus
// (service org.bluez on the system bus). The BlueZ backend talks to it through `busctl` run by a
// `Runner`; tests use a fake `Bluetooth` or canned busctl output, never the real adapter.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runner.hpp"

namespace opensu::host {

struct BluetoothAdapter {
    /// The D-Bus object path, "/org/bluez/hci0".
    std::string path;
    std::string name;
    std::string address;
    bool powered{false};
    bool discovering{false};

    bool operator==(const BluetoothAdapter&) const = default;
};

struct BluetoothDevice {
    /// The D-Bus object path, "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF".
    std::string path;
    std::string address;
    /// The alias BlueZ shows; the address when the device has no name.
    std::string name;
    /// BlueZ's icon name ("input-gaming", "audio-headset", "input-keyboard"), or empty.
    std::string icon;
    bool paired{false};
    bool connected{false};
    bool trusted{false};
    /// 0 to 100 when the device reports its battery (org.bluez.Battery1).
    std::optional<int> battery;

    bool operator==(const BluetoothDevice&) const = default;
};

/// What BlueZ holds now.
struct BluetoothState {
    /// The first adapter; nothing when the machine has none or BlueZ is not running.
    std::optional<BluetoothAdapter> adapter;
    /// Paired devices first, then the connected, then by name. Only devices of `adapter`.
    std::vector<BluetoothDevice> devices;
    /// Why there is no adapter ("BlueZ is not running", "no Bluetooth adapter"); empty with one.
    std::string unavailable;

    bool operator==(const BluetoothState&) const = default;
};

/// Every change returns an empty string when done, else why it was refused.
class Bluetooth {
  public:
    virtual ~Bluetooth() = default;

    [[nodiscard]] virtual BluetoothState read() = 0;
    virtual std::string setPowered(bool powered) = 0;
    /// Scans for 60 s, or until `stopDiscovery`; a second call while scanning does nothing.
    virtual std::string startDiscovery() = 0;
    virtual std::string stopDiscovery() = 0;
    /// Pairs with the device at `path`; can take half a minute, so callers run it off the loop.
    virtual std::string pair(const std::string& path) = 0;
    virtual std::string connect(const std::string& path) = 0;
    virtual std::string disconnect(const std::string& path) = 0;
    virtual std::string setTrusted(const std::string& path, bool trusted) = 0;
    /// Forgets the device: removes it from the adapter.
    virtual std::string remove(const std::string& path) = 0;
};

/// BlueZ over the system bus, through `busctl`. BlueZ ends a scan when the client that started it
/// leaves the bus, so a scan is a held `bluetoothctl --timeout 60 scan on` from `spawn`; ending it
/// (stopDiscovery or destroying the backend) stops the scan.
[[nodiscard]] std::unique_ptr<Bluetooth> makeBluez(Runner run, Spawner spawn);

/// Wraps `inner` so it can be read and nothing else: every change is refused with `reason`. For
/// hidden runs, which must never start a scan or touch the player's devices.
[[nodiscard]] std::unique_ptr<Bluetooth> makeReadOnlyBluetooth(std::unique_ptr<Bluetooth> inner,
                                                               std::string reason);

/// `busctl --json=short call org.bluez / org.freedesktop.DBus.ObjectManager GetManagedObjects`
/// output as a state.
[[nodiscard]] BluetoothState parseManagedObjects(std::string_view json);

} // namespace opensu::host

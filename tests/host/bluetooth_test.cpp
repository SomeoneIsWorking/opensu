// The BlueZ backend against canned busctl output: parsing, the commands each change issues, error
// trimming, path validation and the read-only wrapper. Nothing here reaches the real bus.
#include "host/bluetooth.hpp"
#include "ui/check.hpp"

#include <string>
#include <vector>

namespace {

using opensu::host::Bluetooth;
using opensu::host::BluetoothState;
using opensu::host::Holder;
using opensu::host::Runner;
using opensu::host::Spawner;
using opensu::launch::Captured;
using opensu::test::expect;
using opensu::test::need;
using Args = std::vector<std::string>;

inline constexpr const char* adapterPath = "/org/bluez/hci0";
inline constexpr const char* padPath = "/org/bluez/hci0/dev_86_10_67_67_72_5F";
inline constexpr const char* phonePath = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01";
inline constexpr const char* earsPath = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_02";
inline constexpr const char* foundPath = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_03";

// clang-format off
inline constexpr const char* managed = R"json({"type":"a{oa{sa{sv}}}","data":[{
"/org/bluez":{"org.bluez.AgentManager1":{}},
"/org/bluez/hci0":{"org.bluez.Adapter1":{"Address":{"type":"s","data":"E8:48:B8:C8:20:00"},
 "Name":{"type":"s","data":"fedora"},"Alias":{"type":"s","data":"desk"},
 "Powered":{"type":"b","data":true},"Discovering":{"type":"b","data":true}}},
"/org/bluez/hci0/dev_86_10_67_67_72_5F":{"org.bluez.Device1":{
 "Address":{"type":"s","data":"86:10:67:67:72:5F"},"Alias":{"type":"s","data":"Joy-Con(R)"},
 "Icon":{"type":"s","data":"input-gaming"},"Paired":{"type":"b","data":true},
 "Trusted":{"type":"b","data":true},"Connected":{"type":"b","data":false}}},
"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01":{"org.bluez.Device1":{
 "Address":{"type":"s","data":"AA:BB:CC:DD:EE:01"},"Name":{"type":"s","data":"Phone"},
 "Paired":{"type":"b","data":true},"Connected":{"type":"b","data":true}},
 "org.bluez.Battery1":{"Percentage":{"type":"y","data":80}}},
"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_02":{"org.bluez.Device1":{
 "Address":{"type":"s","data":"AA:BB:CC:DD:EE:02"},"Alias":{"type":"s","data":"Ears"},
 "Paired":{"type":"b","data":true},"Connected":{"type":"b","data":false}}},
"/org/bluez/hci0/dev_AA_BB_CC_DD_EE_03":{"org.bluez.Device1":{
 "Address":{"type":"s","data":"AA:BB:CC:DD:EE:03"},"Paired":{"type":"x","data":1}}},
"/org/bluez/hci1/dev_AA_BB_CC_DD_EE_04":{"org.bluez.Device1":{
 "Address":{"type":"s","data":"AA:BB:CC:DD:EE:04"}}}
}]})json";
// clang-format on

/// Records every program held and counts the holders ended.
struct Spawns {
    std::vector<std::pair<std::string, Args>> started;
    int ended = 0;
    bool missing = false;

    class Counted final : public Holder {
      public:
        explicit Counted(int& ended) : ended_{ended} {
        }
        ~Counted() override {
            ++ended_;
        }
        Counted(const Counted&) = delete;
        Counted& operator=(const Counted&) = delete;
        Counted(Counted&&) = delete;
        Counted& operator=(Counted&&) = delete;

      private:
        int& ended_;
    };

    Spawner spawner() {
        return [this](const std::string& program, const Args& args) -> std::unique_ptr<Holder> {
            if (missing) {
                return nullptr;
            }
            started.emplace_back(program, args);
            return std::make_unique<Counted>(ended);
        };
    }
};

/// Records every command and answers each with the next canned result.
struct Script {
    std::vector<Args> calls;
    std::string readOutput = managed;
    int status = 0;
    std::string output;
    bool missing = false;

    Runner runner() {
        return [this](const std::string& program, const Args& args) -> std::optional<Captured> {
            if (missing || program != "busctl") {
                return std::nullopt;
            }
            calls.push_back(args);
            if (args.size() > 6 && args[6] == "GetManagedObjects") {
                return Captured{status, readOutput};
            }
            return Captured{status, output};
        };
    }
};

Args call(std::initializer_list<const char*> tail) {
    Args args{"--system", "--json=short", "call", "org.bluez"};
    args.insert(args.end(), tail.begin(), tail.end());
    return args;
}

void parsesTheAdapterAndSortsDevices() {
    const BluetoothState state = opensu::host::parseManagedObjects(managed);
    expect(state.unavailable.empty() && state.adapter.has_value(), "an adapter is found");
    expect(need(state.adapter, "an adapter").path == adapterPath &&
               need(state.adapter, "an adapter").name == "desk" &&
               need(state.adapter, "an adapter").address == "E8:48:B8:C8:20:00" &&
               need(state.adapter, "an adapter").powered &&
               need(state.adapter, "an adapter").discovering,
           "the adapter's properties are read, alias first");
    expect(state.devices.size() == 4, "only the adapter's devices are listed");
    expect(state.devices[0].path == phonePath && state.devices[0].connected &&
               state.devices[0].battery == 80 && state.devices[0].name == "Phone",
           "a connected paired device is first, named by Name without an Alias, with its battery");
    expect(state.devices[1].path == earsPath && state.devices[2].path == padPath,
           "paired devices follow by name");
    expect(state.devices[2].icon == "input-gaming" && state.devices[2].trusted &&
               !state.devices[2].battery,
           "icon and trust are read");
    expect(state.devices[3].path == foundPath && !state.devices[3].paired &&
               state.devices[3].name == "AA:BB:CC:DD:EE:03",
           "an odd property type reads as false and an unnamed device shows its address");
}

void reportsWhatIsMissing() {
    expect(opensu::host::parseManagedObjects("not json").unavailable == "BlueZ is not running",
           "malformed JSON is BlueZ not running");
    expect(opensu::host::parseManagedObjects(R"({"data":[{"/org/bluez":{}}]})").unavailable ==
               "no Bluetooth adapter",
           "no adapter object");
    Script script;
    script.status = 1;
    Spawns spawns;
    auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
    expect(bluez->read().unavailable == "BlueZ is not running", "a failing busctl is no BlueZ");
    Script gone;
    gone.missing = true;
    expect(opensu::host::makeBluez(gone.runner(), spawns.spawner())->read().unavailable ==
               "BlueZ is not running",
           "a missing busctl is no BlueZ");
}

void issuesTheCommands() {
    Script script;
    Spawns spawns;
    auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
    expect(bluez->read().adapter.has_value(), "read goes through busctl");
    expect(script.calls.back() == Args({"--system", "--json=short", "call", "org.bluez", "/",
                                        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"}),
           "read asks the object manager");

    expect(bluez->setPowered(false).empty(), "power sets");
    expect(script.calls.back() == call({"/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Set",
                                        "ssv", "org.bluez.Adapter1", "Powered", "b", "false"}),
           "Powered through Properties.Set");
    expect(bluez->pair(padPath).empty() &&
               script.calls.back() == call({padPath, "org.bluez.Device1", "Pair"}),
           "Pair");
    expect(bluez->connect(padPath).empty() &&
               script.calls.back() == call({padPath, "org.bluez.Device1", "Connect"}),
           "Connect");
    expect(bluez->disconnect(padPath).empty() &&
               script.calls.back() == call({padPath, "org.bluez.Device1", "Disconnect"}),
           "Disconnect");
    expect(bluez->setTrusted(padPath, true).empty() &&
               script.calls.back() == call({padPath, "org.freedesktop.DBus.Properties", "Set",
                                            "ssv", "org.bluez.Device1", "Trusted", "b", "true"}),
           "Trusted through Properties.Set");
    expect(bluez->remove(padPath).empty() &&
               script.calls.back() ==
                   call({"/org/bluez/hci0", "org.bluez.Adapter1", "RemoveDevice", "o", padPath}),
           "RemoveDevice on the adapter");
}

void trimsRefusals() {
    Script script;
    Spawns spawns;
    auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
    script.status = 1;
    script.output = "Call failed: Authentication Failed\n";
    expect(bluez->pair(padPath) == "Authentication Failed", "BlueZ's reason is kept");
    script.output = "Failed to call method: Page Timeout\nmore\n";
    expect(bluez->connect(padPath) == "Page Timeout", "only the first line, without the noise");
    script.output = "\n";
    expect(bluez->connect(padPath) == "BlueZ refused", "an empty reason still refuses");
}

void refusesBadPaths() {
    Script script;
    Spawns spawns;
    auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
    for (const char* bad : {"", "/org/bluez/", "/etc/passwd", "/org/bluez/hci0/dev;rm", "--help",
                            "/org/bluez/hci0/dev x"}) {
        expect(!bluez->pair(bad).empty() && !bluez->remove(bad).empty() &&
                   !bluez->setTrusted(bad, true).empty(),
               "a path that is not BlueZ's is refused");
    }
    expect(script.calls.empty(), "a refused path runs nothing");
    Script none;
    none.readOutput = R"({"data":[{}]})";
    expect(opensu::host::makeBluez(none.runner(), spawns.spawner())->startDiscovery() ==
                   "no Bluetooth adapter" &&
               spawns.started.empty(),
           "a scan without an adapter is refused and holds nothing");
}

void holdsTheScan() {
    Script script;
    Spawns spawns;
    {
        auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
        expect(bluez->stopDiscovery().empty() && spawns.ended == 0,
               "stopping without a scan does nothing");
        expect(bluez->startDiscovery().empty(), "a scan starts");
        expect(spawns.started.size() == 1 && spawns.started[0].first == "bluetoothctl" &&
                   spawns.started[0].second == Args({"--timeout", "60", "scan", "on"}),
               "the scan is a held bluetoothctl");
        expect(bluez->startDiscovery().empty() && spawns.started.size() == 1,
               "a second start while scanning holds nothing more");
        expect(bluez->stopDiscovery().empty() && spawns.ended == 1, "stopping ends the holder");
        expect(bluez->startDiscovery().empty() && spawns.started.size() == 2,
               "a scan starts again after a stop");
        for (const Args& args : script.calls) {
            expect(args.size() < 7 || (args[6] != "StartDiscovery" && args[6] != "StopDiscovery"),
                   "no discovery call goes through busctl");
        }
    }
    expect(spawns.ended == 2, "destroying the backend ends a running scan");
    spawns.missing = true;
    auto bluez = opensu::host::makeBluez(script.runner(), spawns.spawner());
    expect(bluez->startDiscovery() == "scanning needs bluetoothctl (package bluez)",
           "without bluetoothctl a scan is refused with the package");
    expect(bluez->stopDiscovery().empty(), "and there is nothing to stop");
}

/// Counts what reaches it.
class Counting final : public Bluetooth {
  public:
    int changes = 0;
    int reads = 0;
    BluetoothState read() override {
        ++reads;
        return {};
    }
    std::string setPowered(bool) override {
        ++changes;
        return {};
    }
    std::string startDiscovery() override {
        ++changes;
        return {};
    }
    std::string stopDiscovery() override {
        ++changes;
        return {};
    }
    std::string pair(const std::string&) override {
        ++changes;
        return {};
    }
    std::string connect(const std::string&) override {
        ++changes;
        return {};
    }
    std::string disconnect(const std::string&) override {
        ++changes;
        return {};
    }
    std::string setTrusted(const std::string&, bool) override {
        ++changes;
        return {};
    }
    std::string remove(const std::string&) override {
        ++changes;
        return {};
    }
};

void readOnlyNeverChanges() {
    auto inner = std::make_unique<Counting>();
    const Counting& seen = *inner;
    auto guarded = opensu::host::makeReadOnlyBluetooth(std::move(inner), "hidden run");
    (void)guarded->read();
    expect(seen.reads == 1, "reads pass through");
    expect(guarded->setPowered(true) == "hidden run" && guarded->startDiscovery() == "hidden run" &&
               guarded->stopDiscovery() == "hidden run" && guarded->pair("p") == "hidden run" &&
               guarded->connect("p") == "hidden run" && guarded->disconnect("p") == "hidden run" &&
               guarded->setTrusted("p", true) == "hidden run" &&
               guarded->remove("p") == "hidden run",
           "every change is refused with the reason");
    expect(seen.changes == 0, "no change reaches the inner backend");
}

} // namespace

int main() {
    parsesTheAdapterAndSortsDevices();
    reportsWhatIsMissing();
    issuesTheCommands();
    trimsRefusals();
    refusesBadPaths();
    holdsTheScan();
    readOnlyNeverChanges();
    return 0;
}

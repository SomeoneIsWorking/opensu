#include "bluetooth.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include <nlohmann/json.hpp>

namespace opensu::host {
namespace {

using json = nlohmann::json;

constexpr const char* service = "org.bluez";
constexpr const char* adapterInterface = "org.bluez.Adapter1";
constexpr const char* deviceInterface = "org.bluez.Device1";
constexpr const char* batteryInterface = "org.bluez.Battery1";
constexpr const char* propertiesInterface = "org.freedesktop.DBus.Properties";
constexpr const char* pathPrefix = "/org/bluez/";

const json none;

/// The `data` of property `name` in `properties`, or null when absent or malformed.
const json& property(const json& properties, const char* name) {
    if (!properties.is_object()) {
        return none;
    }
    const auto found = properties.find(name);
    if (found == properties.end() || !found->is_object()) {
        return none;
    }
    const auto data = found->find("data");
    return data == found->end() ? none : *data;
}

std::string textOf(const json& properties, const char* name) {
    const json& value = property(properties, name);
    return value.is_string() ? value.get<std::string>() : std::string{};
}

bool flagOf(const json& properties, const char* name) {
    const json& value = property(properties, name);
    return value.is_boolean() && value.get<bool>();
}

const json& interfaceOf(const json& object, const char* name) {
    if (!object.is_object()) {
        return none;
    }
    const auto found = object.find(name);
    return found == object.end() ? none : *found;
}

BluetoothDevice deviceOf(const std::string& path, const json& object) {
    const json& device = interfaceOf(object, deviceInterface);
    BluetoothDevice made;
    made.path = path;
    made.address = textOf(device, "Address");
    made.name = textOf(device, "Alias");
    if (made.name.empty()) {
        made.name = textOf(device, "Name");
    }
    if (made.name.empty()) {
        made.name = made.address;
    }
    made.icon = textOf(device, "Icon");
    made.paired = flagOf(device, "Paired");
    made.connected = flagOf(device, "Connected");
    made.trusted = flagOf(device, "Trusted");
    const json& level = property(interfaceOf(object, batteryInterface), "Percentage");
    if (level.is_number_integer()) {
        made.battery = std::clamp(level.get<int>(), 0, 100);
    }
    return made;
}

/// Whether `path` is a BlueZ object path that is safe to put on a command line.
bool validPath(const std::string& path) {
    return path.starts_with(pathPrefix) &&
           path.size() > std::char_traits<char>::length(pathPrefix) &&
           std::ranges::all_of(path, [](char c) {
               return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '/';
           });
}

/// busctl's complaint as one clean line.
std::string reasonOf(std::string output) {
    constexpr const char* noise[] = {
        "Failed to call method:", "Call failed:", "Failed to set property:"};
    const std::size_t end = output.find('\n');
    if (end != std::string::npos) {
        output.resize(end);
    }
    for (const char* prefix : noise) {
        if (output.starts_with(prefix)) {
            output.erase(0, std::char_traits<char>::length(prefix));
        }
    }
    const auto blank = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
    output.erase(output.begin(), std::ranges::find_if_not(output, blank));
    while (!output.empty() && blank(output.back())) {
        output.pop_back();
    }
    return output.empty() ? "BlueZ refused" : output;
}

class Bluez final : public Bluetooth {
  public:
    Bluez(Runner run, Spawner spawn) : run_{std::move(run)}, spawn_{std::move(spawn)} {
    }

    BluetoothState read() override {
        BluetoothState unreachable;
        unreachable.unavailable = "BlueZ is not running";
        const std::optional<launch::Captured> out =
            run_("busctl", {"--system", "--json=short", "call", service, "/",
                            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"});
        if (!out || out->status != 0) {
            return unreachable;
        }
        return parseManagedObjects(out->output);
    }
    std::string setPowered(bool powered) override {
        return setAdapterProperty("Powered", powered);
    }
    std::string startDiscovery() override {
        if (scan_) {
            return {};
        }
        if (adapterPath().empty()) {
            return "no Bluetooth adapter";
        }
        scan_ = spawn_("bluetoothctl", {"--timeout", "60", "scan", "on"});
        return scan_ ? std::string{} : "scanning needs bluetoothctl (package bluez)";
    }
    std::string stopDiscovery() override {
        scan_.reset();
        return {};
    }
    std::string pair(const std::string& path) override {
        return callDevice(path, "Pair");
    }
    std::string connect(const std::string& path) override {
        return callDevice(path, "Connect");
    }
    std::string disconnect(const std::string& path) override {
        return callDevice(path, "Disconnect");
    }
    std::string setTrusted(const std::string& path, bool trusted) override {
        if (!validPath(path)) {
            return invalid();
        }
        return finish(call({path, propertiesInterface, "Set", "ssv", deviceInterface, "Trusted",
                            "b", trusted ? "true" : "false"}));
    }
    std::string remove(const std::string& path) override {
        if (!validPath(path)) {
            return invalid();
        }
        const std::string adapter = adapterPath();
        if (adapter.empty()) {
            return "no Bluetooth adapter";
        }
        return finish(call({adapter, adapterInterface, "RemoveDevice", "o", path}));
    }

  private:
    static std::string invalid() {
        return "not a Bluetooth device";
    }

    std::optional<launch::Captured> call(std::vector<std::string> arguments) {
        std::vector<std::string> full{"--system", "--json=short", "call", service};
        full.insert(full.end(), std::make_move_iterator(arguments.begin()),
                    std::make_move_iterator(arguments.end()));
        return run_("busctl", full);
    }

    static std::string finish(const std::optional<launch::Captured>& out) {
        if (!out) {
            return "busctl is not installed";
        }
        return out->status == 0 ? std::string{} : reasonOf(out->output);
    }

    std::string adapterPath() {
        const BluetoothState state = read();
        return state.adapter ? state.adapter->path : std::string{};
    }

    std::string setAdapterProperty(const char* name, bool value) {
        const std::string adapter = adapterPath();
        if (adapter.empty()) {
            return "no Bluetooth adapter";
        }
        return finish(call({adapter, propertiesInterface, "Set", "ssv", adapterInterface, name, "b",
                            value ? "true" : "false"}));
    }

    std::string callDevice(const std::string& path, const char* method) {
        if (!validPath(path)) {
            return invalid();
        }
        return finish(call({path, deviceInterface, method}));
    }

    Runner run_;
    Spawner spawn_;
    /// The held scan; destroying it stops the scan.
    std::unique_ptr<Holder> scan_;
};

class ReadOnlyBluetooth final : public Bluetooth {
  public:
    ReadOnlyBluetooth(std::unique_ptr<Bluetooth> inner, std::string reason)
        : inner_{std::move(inner)}, reason_{std::move(reason)} {
    }

    BluetoothState read() override {
        return inner_->read();
    }
    std::string setPowered(bool) override {
        return reason_;
    }
    std::string startDiscovery() override {
        return reason_;
    }
    std::string stopDiscovery() override {
        return reason_;
    }
    std::string pair(const std::string&) override {
        return reason_;
    }
    std::string connect(const std::string&) override {
        return reason_;
    }
    std::string disconnect(const std::string&) override {
        return reason_;
    }
    std::string setTrusted(const std::string&, bool) override {
        return reason_;
    }
    std::string remove(const std::string&) override {
        return reason_;
    }

  private:
    std::unique_ptr<Bluetooth> inner_;
    std::string reason_;
};

} // namespace

BluetoothState parseManagedObjects(std::string_view text) {
    BluetoothState state;
    state.unavailable = "BlueZ is not running";
    const json parsed = json::parse(text, nullptr, false);
    if (!parsed.is_object() || !parsed.contains("data") || !parsed["data"].is_array() ||
        parsed["data"].empty() || !parsed["data"][0].is_object()) {
        return state;
    }
    const json& objects = parsed["data"][0];
    state.unavailable = "no Bluetooth adapter";
    for (const auto& [path, object] : objects.items()) {
        if (!object.is_object() || !object.contains(adapterInterface)) {
            continue;
        }
        if (state.adapter && path >= state.adapter->path) {
            continue;
        }
        const json& adapter = object[adapterInterface];
        BluetoothAdapter made;
        made.path = path;
        made.address = textOf(adapter, "Address");
        made.name = textOf(adapter, "Alias");
        if (made.name.empty()) {
            made.name = textOf(adapter, "Name");
        }
        made.powered = flagOf(adapter, "Powered");
        made.discovering = flagOf(adapter, "Discovering");
        state.adapter = std::move(made);
    }
    if (!state.adapter) {
        return state;
    }
    state.unavailable.clear();
    const std::string prefix = state.adapter->path + "/";
    for (const auto& [path, object] : objects.items()) {
        if (path.starts_with(prefix) && object.is_object() && object.contains(deviceInterface)) {
            state.devices.push_back(deviceOf(path, object));
        }
    }
    std::ranges::sort(state.devices, [](const BluetoothDevice& a, const BluetoothDevice& b) {
        if (a.paired != b.paired) {
            return a.paired;
        }
        if (a.connected != b.connected) {
            return a.connected;
        }
        return a.name != b.name ? a.name < b.name : a.path < b.path;
    });
    return state;
}

std::unique_ptr<Bluetooth> makeBluez(Runner run, Spawner spawn) {
    return std::make_unique<Bluez>(std::move(run), std::move(spawn));
}

std::unique_ptr<Bluetooth> makeReadOnlyBluetooth(std::unique_ptr<Bluetooth> inner,
                                                 std::string reason) {
    return std::make_unique<ReadOnlyBluetooth>(std::move(inner), std::move(reason));
}

} // namespace opensu::host

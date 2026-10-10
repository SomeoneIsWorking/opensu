// Fakes of the machine's services and the mixer for the controller tests: they record what was
// asked and never touch the machine.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/volume_backend.hpp"
#include "host/backlight.hpp"
#include "host/bluetooth.hpp"
#include "host/power.hpp"
#include "host/session_mode.hpp"

namespace opensu::test {

class FakePower final : public host::Power {
  public:
    std::string perform(host::PowerAction action) override {
        performed.push_back(action);
        return refusal;
    }

    std::vector<host::PowerAction> performed;
    std::string refusal;
};

class FakeSessionMode final : public host::SessionMode {
  public:
    bool installed() const override {
        return isInstalled;
    }
    std::string installBlocker() const override {
        return blocker;
    }
    host::InstallResult install(std::string_view password) override {
        passwords.emplace_back(password);
        if (results.empty()) {
            return {};
        }
        const host::InstallResult next = results.front();
        results.erase(results.begin());
        return next;
    }
    std::string switchToSession() override {
        calls.emplace_back("session");
        return refusal;
    }
    std::string switchToDesktop() override {
        calls.emplace_back("desktop");
        return refusal;
    }

    bool isInstalled{false};
    std::string blocker;
    std::string refusal;
    /// The results the installs give, in order; a missing one is a success.
    std::vector<host::InstallResult> results;
    std::vector<std::string> passwords;
    std::vector<std::string> calls;
};

class FakeBluetooth final : public host::Bluetooth {
  public:
    host::BluetoothState read() override {
        return state;
    }
    std::string setPowered(bool powered) override {
        return note("power " + std::to_string(powered), [&] {
            state.adapter->powered = powered;
        });
    }
    std::string startDiscovery() override {
        return note("scan", [&] {
            state.adapter->discovering = true;
        });
    }
    std::string stopDiscovery() override {
        return note("stop", [&] {
            state.adapter->discovering = false;
        });
    }
    std::string pair(const std::string& path) override {
        return note("pair " + path, [&] {
            find(path).paired = true;
        });
    }
    std::string connect(const std::string& path) override {
        return note("connect " + path, [&] {
            find(path).connected = true;
        });
    }
    std::string disconnect(const std::string& path) override {
        return note("disconnect " + path, [&] {
            find(path).connected = false;
        });
    }
    std::string setTrusted(const std::string& path, bool trusted) override {
        return note("trust " + path, [&] {
            find(path).trusted = trusted;
        });
    }
    std::string remove(const std::string& path) override {
        return note("remove " + path, [&] {
            std::erase_if(state.devices, [&](const host::BluetoothDevice& device) {
                return device.path == path;
            });
        });
    }

    host::BluetoothState state;
    std::vector<std::string> log;
    std::string refusal;

  private:
    host::BluetoothDevice& find(const std::string& path) {
        for (host::BluetoothDevice& device : state.devices) {
            if (device.path == path) {
                return device;
            }
        }
        return state.devices.front();
    }
    template <class Apply> std::string note(std::string what, Apply apply) {
        log.push_back(std::move(what));
        if (refusal.empty()) {
            apply();
        }
        return refusal;
    }
};

/// An adapter and two devices: a paired pad and a headset not yet paired.
inline host::BluetoothState pairedPadAndHeadset() {
    host::BluetoothState state;
    state.adapter = host::BluetoothAdapter{"/org/bluez/hci0", "hci0", "AA:BB", true, false};
    state.devices = {
        host::BluetoothDevice{"/d/pad", "11:22", "Pro Pad", "input-gaming", true, false, true, 80},
        host::BluetoothDevice{"/d/buds", "33:44", "Buds", "audio-headset", false, false, false,
                              std::nullopt}};
    return state;
}

class FakeBacklight final : public host::Backlight {
  public:
    std::optional<int> percent() override {
        return level;
    }
    std::string setPercent(int next) override {
        level = next;
        return {};
    }

    int level{50};
};

class FakeMixer final : public audio::VolumeBackend {
  public:
    [[nodiscard]] std::string_view name() const override {
        return "fake";
    }
    std::optional<audio::VolumeState> read() override {
        return state;
    }
    bool setPercent(int percent) override {
        state.percent = percent;
        return true;
    }
    bool setMuted(bool muted) override {
        state.muted = muted;
        return true;
    }
    std::vector<audio::AudioSink> sinks() override {
        return outputs;
    }
    bool setDefaultSink(const std::string& id) override {
        for (audio::AudioSink& sink : outputs) {
            sink.isDefault = sink.id == id;
        }
        return true;
    }

    audio::VolumeState state{40, false};
    std::vector<audio::AudioSink> outputs{{"1", "Speakers", true}, {"2", "Headphones", false}};
};

} // namespace opensu::test

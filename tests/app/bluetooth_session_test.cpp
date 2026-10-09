// The Bluetooth session over a fake BlueZ: reads arrive off the loop, changes run one at a time and
// leave a notice, pairing trusts and connects, and a refusal carries its reason.
#include <chrono>
#include <cstdio>
#include <thread>

#include "bluetooth_session.hpp"
#include "host_fakes.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

/// Polls until the read or change in flight has been taken.
void settle(app::BluetoothSession& session) {
    for (int attempt = 0; attempt < 2000; ++attempt) {
        static_cast<void>(session.poll(app::BluetoothSession::Clock::now(), true));
        if (session.known() && session.busyWith().empty()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    expect(false, "the session settled");
}

void readsOnlyWhenWanted() {
    test::FakeBluetooth bluetooth;
    bluetooth.state = test::pairedPadAndHeadset();
    app::BluetoothSession session{bluetooth};
    static_cast<void>(session.poll(app::BluetoothSession::Clock::now(), false));
    expect(!session.known(), "nothing is read while nothing shows Bluetooth");
    settle(session);
    expect(session.state().devices.size() == 2 &&
               test::need(session.state().adapter, "an adapter").powered,
           "the state arrives");
    expect(session.batteryOf("11:22") == 80 && !session.batteryOf("33:44"), "batteries by address");
}

void pairingTrustsAndConnects() {
    test::FakeBluetooth bluetooth;
    bluetooth.state = test::pairedPadAndHeadset();
    app::BluetoothSession session{bluetooth};
    settle(session);
    const host::BluetoothDevice buds = session.state().devices[1];
    expect(session.pair(buds).empty(), "pairing starts");
    expect(!session.pair(buds).empty(), "a second change waits for the first");
    settle(session);
    expect(bluetooth.log ==
               std::vector<std::string>{"pair /d/buds", "trust /d/buds", "connect /d/buds"},
           "pair, then trust, then connect");
    const std::optional<app::BluetoothNotice> notice = session.takeNotice();
    expect(notice && !notice->error && notice->text.find("Buds") != std::string::npos,
           "the player is told");
    expect(!session.takeNotice(), "a notice is taken once");
}

void aRefusalCarriesItsReason() {
    test::FakeBluetooth bluetooth;
    bluetooth.state = test::pairedPadAndHeadset();
    bluetooth.refusal = "Authentication Failed";
    app::BluetoothSession session{bluetooth};
    settle(session);
    expect(session.pair(session.state().devices[1]).empty(), "it starts");
    settle(session);
    const std::optional<app::BluetoothNotice> notice = session.takeNotice();
    expect(notice && notice->error &&
               notice->text.find("Authentication Failed") != std::string::npos,
           "the reason is in the notice");
}

void scanTogglesAndForgetRemoves() {
    test::FakeBluetooth bluetooth;
    bluetooth.state = test::pairedPadAndHeadset();
    app::BluetoothSession session{bluetooth};
    settle(session);
    expect(session.toggleScan().empty(), "a scan starts");
    settle(session);
    expect(session.scanning(), "and shows");
    expect(session.forget(session.state().devices[0]).empty(), "forgetting starts");
    settle(session);
    expect(session.state().devices.size() == 1, "the device is gone");
}

} // namespace

int main() {
    readsOnlyWhenWanted();
    pairingTrustsAndConnects();
    aRefusalCarriesItsReason();
    scanTogglesAndForgetRemoves();
    std::printf("bluetooth_session: all checks passed\n");
    return 0;
}

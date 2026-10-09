// The hand-over between the control channel's threads and the loop: queued input, reload requests,
// the published state and the frame handshake.
#include "control_bridge.hpp"

#include <chrono>
#include <string>
#include <thread>

#include "ui/check.hpp"

namespace {

using opensu::app::ControlBridge;
using opensu::gamepad::Button;
using opensu::input::Device;
using opensu::test::expect;

void inputIsTakenOnceInOrder() {
    ControlBridge bridge;
    bridge.inject(Button::A, Device::Pad);
    bridge.inject(Button::B, Device::KeyboardMouse);
    bridge.injectKey(opensu::input::Combo{1});
    bridge.typeText("ab");
    const ControlBridge::Input taken = bridge.takeInput();
    expect(taken.buttons.size() == 2 && taken.buttons[0].first == Button::A &&
               taken.buttons[1].second == Device::KeyboardMouse,
           "buttons arrive in order with their device");
    expect(taken.keys.size() == 1 && taken.text == std::vector<std::string>{"ab"},
           "keys and text arrive");
    const ControlBridge::Input again = bridge.takeInput();
    expect(again.buttons.empty() && again.keys.empty() && again.text.empty(),
           "input is taken once");
}

void reloadAndCloseAreRequests() {
    ControlBridge bridge;
    expect(!bridge.takeReload() && !bridge.closeRequested(), "nothing is requested at first");
    bridge.requestCatalogReload("library refreshed");
    expect(bridge.takeReload() == "library refreshed", "a reload carries its toast");
    expect(!bridge.takeReload(), "a reload is taken once");
    bridge.requestClose();
    expect(bridge.closeRequested(), "close is requested");
}

void snapshotIsWhatWasPublished() {
    ControlBridge bridge;
    opensu::app::ShellSnapshot state;
    state.games = 7;
    bridge.publish(state);
    expect(bridge.snapshot().games == 7, "the published state is read back");
}

void frameIsHandedAcross() {
    ControlBridge bridge;
    std::string png;
    bool got = false;
    std::thread caller{[&] {
        got = bridge.captureFrame(png);
    }};
    while (!bridge.frameWanted()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    bridge.answerFrame("PNG");
    caller.join();
    expect(got && png == "PNG", "the caller gets the frame the loop drew");
    expect(!bridge.frameWanted(), "the request is settled");

    bool failed = true;
    std::thread second{[&] {
        std::string unused;
        failed = !bridge.captureFrame(unused);
    }};
    while (!bridge.frameWanted()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    bridge.answerFrame({});
    second.join();
    expect(failed, "a frame that could not be drawn fails the caller");
}

void abandoningReleasesTheCaller() {
    ControlBridge bridge;
    bool got = true;
    std::thread caller{[&] {
        std::string png;
        got = bridge.captureFrame(png);
    }};
    while (!bridge.frameWanted()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    bridge.abandonFrame();
    caller.join();
    expect(!got, "an abandoned request fails the caller");
}

} // namespace

int main() {
    inputIsTakenOnceInOrder();
    reloadAndCloseAreRequests();
    snapshotIsWhatWasPublished();
    frameIsHandedAcross();
    abandoningReleasesTheCaller();
    return 0;
}

// The hand-over between the control channel's threads and the loop: queued input, reload requests,
// the published state and the frame handshake.
#include "control_bridge.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ui/check.hpp"

namespace {

using opensu::app::ControlBridge;
using opensu::gamepad::Button;
using opensu::input::Device;
using opensu::test::expect;
using namespace std::chrono_literals;

/// Long enough after any queued tap that every edge is due.
ControlBridge::Input takeLater(ControlBridge& bridge) {
    return bridge.takeInput(ControlBridge::Clock::now() + 1s);
}

void inputIsTakenOnceInOrder() {
    ControlBridge bridge;
    bridge.inject({Button::A}, Device::Pad, 0ms);
    bridge.inject({Button::B}, Device::KeyboardMouse, 0ms);
    bridge.injectKey(opensu::input::Combo{1});
    bridge.typeText("ab");
    const ControlBridge::Input taken = takeLater(bridge);
    expect(taken.buttons.size() == 4 && taken.buttons[0].first.button == Button::A &&
               taken.buttons[0].first.pressed && !taken.buttons[1].first.pressed &&
               taken.buttons[3].second == Device::KeyboardMouse,
           "a button is a press and a release, in order with its device");
    expect(taken.keys.size() == 1 && taken.text == std::vector<std::string>{"ab"},
           "keys and text arrive");
    const ControlBridge::Input again = takeLater(bridge);
    expect(again.buttons.empty() && again.keys.empty() && again.text.empty(),
           "input is taken once");
}

void heldButtonsAreAChord() {
    ControlBridge bridge;
    bridge.inject({Button::Guide, Button::A}, Device::Pad, 0ms);
    const ControlBridge::Input taken = takeLater(bridge);
    std::vector<std::pair<Button, bool>> edges;
    edges.reserve(taken.buttons.size());
    for (const auto& [event, device] : taken.buttons) {
        edges.emplace_back(event.button, event.pressed);
    }
    expect(edges == std::vector<std::pair<Button, bool>>{{Button::Guide, true},
                                                         {Button::A, true},
                                                         {Button::A, false},
                                                         {Button::Guide, false}},
           "held buttons go down in order and come up in reverse");
}

void aHoldKeepsTheReleaseBack() {
    ControlBridge bridge;
    const ControlBridge::Clock::time_point start = ControlBridge::Clock::now();
    bridge.inject({Button::Guide}, Device::Pad, 3s);
    bridge.inject({Button::A}, Device::Pad, 0ms);
    const ControlBridge::Input first = bridge.takeInput(start + 1s);
    expect(first.buttons.size() == 1 && first.buttons[0].first.button == Button::Guide &&
               first.buttons[0].first.pressed,
           "a held button is pressed at once, and nothing queued after it overtakes its release");
    const ControlBridge::Input later = bridge.takeInput(start + 4s);
    expect(later.buttons.size() == 3 && later.buttons[0].first.button == Button::Guide &&
               !later.buttons[0].first.pressed && later.buttons[1].first.button == Button::A,
           "once the hold is over it is released, then the rest follows");
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
    heldButtonsAreAChord();
    aHoldKeepsTheReleaseBack();
    reloadAndCloseAreRequests();
    snapshotIsWhatWasPublished();
    frameIsHandedAcross();
    abandoningReleasesTheCaller();
    return 0;
}

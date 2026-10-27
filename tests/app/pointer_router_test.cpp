// The pointer router: what a frame of the mouse asks of the shell.
#include "pointer_router.hpp"

#include <cstdio>
#include <string>
#include <variant>
#include <vector>

#include "ui/check.hpp"

namespace {

using opensu::app::PointerFrame;
using opensu::app::PointerHost;
using opensu::app::PointerRouter;
using opensu::gamepad::Button;
using opensu::input::Device;
using opensu::input::LastDevice;
using opensu::test::expect;
using opensu::ui::PointerTarget;

/// Records what the router asks, over a pointer that is always on `under`.
class Recorder final : public PointerHost {
  public:
    PointerTarget under;
    std::vector<std::string> calls;
    bool pointerGiven{false};
    PointerTarget menuTarget;

    PointerTarget pointAt(std::optional<opensu::ui::Point> point) override {
        pointerGiven = point.has_value();
        return point ? under : PointerTarget{};
    }
    void focus(const PointerTarget&) override {
        calls.emplace_back("focus");
    }
    void press(Button button) override {
        calls.emplace_back(button == Button::A ? "A" : (button == Button::B ? "B" : "other"));
    }
    void activateSection(opensu::library::Section) override {
        calls.emplace_back("section");
    }
    void selectLauncher(opensu::library::Source) override {
        calls.emplace_back("launcher");
    }
    void openDownloads() override {
        calls.emplace_back("downloads");
    }
    void activateCrumb(std::size_t index) override {
        calls.push_back("crumb" + std::to_string(index));
    }
    void contextMenu(const PointerTarget& target) override {
        calls.emplace_back("menu");
        menuTarget = target;
    }
    void scroll(int steps) override {
        calls.emplace_back(steps > 0 ? "scroll+" : "scroll-");
    }
    void screens(const PointerFrame& frame) override {
        calls.emplace_back(frame.left ? "screens click" : "screens");
    }
};

PointerFrame at(float x, float y, float dx = 0.0f) {
    PointerFrame frame;
    frame.point = opensu::ui::Point{x, y};
    frame.delta = opensu::ui::Point{dx, 0.0f};
    return frame;
}

void hoverFocusesOnlyWhenTheMouseMoved() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    host.under = opensu::ui::OnTile{3};
    router.route(at(10.0f, 10.0f));
    expect(host.calls.empty(), "a pointer that did not move changes nothing");
    expect(device.current() == Device::Pad && !host.pointerGiven, "and does not take the device");
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls == std::vector<std::string>{"focus"}, "a moving pointer focuses the tile");
    host.calls.clear();
    router.route(at(10.0f, 10.0f));
    expect(host.calls.empty(), "resting on it again does not refocus");
}

void hoverSkipsWhatAHoverWouldTrigger() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    host.under = opensu::ui::OnPage{2};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a page arrow turns only on a click");
    host.under = opensu::ui::OnDock{opensu::library::Section::Library};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a dock item only lights");
    host.under = opensu::ui::OnLauncher{opensu::library::Source::Gog};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a launcher's badge only lights");
    host.under = opensu::ui::OnCrumb{0};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a breadcrumb only lights");
}

void leftClickIsFocusThenA() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    for (const PointerTarget target :
         {PointerTarget{opensu::ui::OnTile{1}}, PointerTarget{opensu::ui::OnKeyboardKey{0}}}) {
        host.under = target;
        host.calls.clear();
        PointerFrame frame = at(10.0f, 10.0f);
        frame.left = true;
        router.route(frame);
        expect(host.calls == std::vector<std::string>{"focus", "A"}, "focus, then the A press");
    }
    expect(device.current() == Device::KeyboardMouse, "a click makes the mouse the device");
}

void leftClickOnTheOthers() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    PointerFrame frame = at(10.0f, 10.0f);
    frame.left = true;
    host.under = opensu::ui::OnDock{opensu::library::Section::Home};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"section"}, "a dock item changes section");
    host.calls.clear();
    host.under = opensu::ui::OnPage{1};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"focus"}, "a page control turns the page");
    host.calls.clear();
    host.under = opensu::ui::OnLauncher{opensu::library::Source::Epic};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"launcher"},
           "a launcher's badge selects its store");
    host.calls.clear();
    host.under = opensu::ui::OnLauncher{opensu::library::Source::Steam, true};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"downloads"},
           "a badge showing a download opens the Downloads page");
    host.calls.clear();
    host.under = opensu::ui::OnCrumb{1};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"crumb1"}, "a breadcrumb goes back to its level");
    host.calls.clear();
    host.under = PointerTarget{};
    router.route(frame);
    expect(host.calls.empty(), "a click on nothing does nothing");
}

void rightClickAndWheel() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    PointerFrame frame = at(10.0f, 10.0f);
    frame.right = true;
    host.under = opensu::ui::OnTile{4};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"menu"}, "a right click asks for a menu");
    const auto* menuTile = std::get_if<opensu::ui::OnTile>(&host.menuTarget);
    expect(menuTile != nullptr && menuTile->index == 4, "the menu is for the target");
    host.calls.clear();
    host.under = opensu::ui::OnKeyboardKey{1};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"menu"}, "a right click is never B");
    host.calls.clear();
    frame = at(10.0f, 10.0f);
    frame.wheel = -1.0f;
    router.route(frame);
    frame.wheel = 2.0f;
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"scroll+", "scroll-"},
           "the wheel steps down, then up");
    expect(device.current() == Device::KeyboardMouse, "the wheel is real input");
}

void hoverFocusesAKeyboardKey() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    host.under = opensu::ui::OnKeyboardKey{2};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls == std::vector<std::string>{"focus"}, "hover focuses a keyboard key");
}

void padKeepsFocusFromAStillPointer() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    host.under = opensu::ui::OnTile{0};
    router.route(at(10.0f, 10.0f, 3.0f));
    host.calls.clear();
    opensu::gamepad::Event press;
    press.button = Button::Down;
    press.pressed = true;
    device.notePad(press);
    router.route(at(10.0f, 10.0f));
    expect(host.calls.empty() && !host.pointerGiven,
           "after the pad, a pointer at rest neither focuses nor hovers");
}

/// Over the interface toolkit's screens every frame goes to them whole, moved or not, and nothing
/// else is asked.
void screensTakeThePointerWhole() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    host.under = opensu::ui::OnScreens{};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls == std::vector<std::string>{"screens"}, "a move goes to the screens");
    host.calls.clear();
    PointerFrame click = at(10.0f, 10.0f);
    click.left = true;
    router.route(click);
    expect(host.calls == std::vector<std::string>{"screens click"},
           "a click goes to the screens, not as focus then A");
}

} // namespace

int main() {
    hoverFocusesOnlyWhenTheMouseMoved();
    hoverSkipsWhatAHoverWouldTrigger();
    leftClickIsFocusThenA();
    leftClickOnTheOthers();
    rightClickAndWheel();
    hoverFocusesAKeyboardKey();
    padKeepsFocusFromAStillPointer();
    screensTakeThePointerWhole();
    std::printf("pointer_router: all checks passed\n");
    return 0;
}

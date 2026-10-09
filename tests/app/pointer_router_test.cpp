// The pointer router: what a frame of the mouse asks of the shell.
#include "pointer_router.hpp"

#include <cstdio>
#include <string>
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

    PointerTarget pointAt(std::optional<Vector2> point) override {
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
    void chooseIconSize(int level) override {
        calls.push_back("size" + std::to_string(level));
    }
    void contextMenu(const PointerTarget& target) override {
        calls.emplace_back("menu");
        menuTarget = target;
    }
    void scroll(int steps) override {
        calls.emplace_back(steps > 0 ? "scroll+" : "scroll-");
    }
};

PointerFrame at(float x, float y, float dx = 0.0f) {
    PointerFrame frame;
    frame.point = Vector2{x, y};
    frame.delta = Vector2{dx, 0.0f};
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
    host.under = opensu::ui::OnPanelButton{Button::A};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a panel hint only presses on a click");
    host.under = opensu::ui::OnLauncher{opensu::library::Source::Gog};
    router.route(at(10.0f, 10.0f, 4.0f));
    expect(host.calls.empty(), "a launcher's badge only lights");
}

void leftClickIsFocusThenA() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    for (const PointerTarget target :
         {PointerTarget{opensu::ui::OnTile{1}},
          PointerTarget{opensu::ui::OnLayoutCard{opensu::library::LibraryMode::Xmb}},
          PointerTarget{opensu::ui::OnMenuItem{1}},
          PointerTarget{opensu::ui::OnChooserRow{opensu::ui::ChooserRow::Pin}}}) {
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
    host.under = opensu::ui::OnPanelButton{Button::X};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"other"}, "a panel hint presses its button");
    host.calls.clear();
    host.under = opensu::ui::OnLauncher{opensu::library::Source::Epic};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"launcher"},
           "a launcher's badge selects its store");
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
    expect(host.menuTarget == PointerTarget{opensu::ui::OnTile{4}}, "the menu is for the target");
    host.calls.clear();
    host.under = opensu::ui::OnContextBackdrop{};
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

void iconSizeAndBackdropClicks() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    PointerFrame frame = at(10.0f, 10.0f);
    frame.left = true;
    host.under = opensu::ui::OnIconSize{14};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"focus", "size14"}, "the slider sets the level");
    host.calls.clear();
    host.under = opensu::ui::OnContextBackdrop{};
    router.route(frame);
    expect(host.calls == std::vector<std::string>{"B"}, "a click outside a menu closes it");
}

void hoverFocusesThePanelTargets() {
    Recorder host;
    LastDevice device;
    PointerRouter router{device, host};
    for (const PointerTarget target :
         {PointerTarget{opensu::ui::OnIconSize{3}}, PointerTarget{opensu::ui::OnSearchKey{2}},
          PointerTarget{opensu::ui::OnSearchResult{1}},
          PointerTarget{opensu::ui::OnContextItem{0}}}) {
        host.under = target;
        host.calls.clear();
        router.route(at(10.0f, 10.0f, 4.0f));
        expect(host.calls == std::vector<std::string>{"focus"}, "hover focuses a panel target");
    }
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

} // namespace

int main() {
    hoverFocusesOnlyWhenTheMouseMoved();
    hoverSkipsWhatAHoverWouldTrigger();
    leftClickIsFocusThenA();
    leftClickOnTheOthers();
    rightClickAndWheel();
    iconSizeAndBackdropClicks();
    hoverFocusesThePanelTargets();
    padKeepsFocusFromAStillPointer();
    std::printf("pointer_router: all checks passed\n");
    return 0;
}

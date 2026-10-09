// Shift+Tab as Guide: the chord's edges, and the raw key path against a real X server (Xvfb,
// keys injected with XTest).
#include "session/game_keys.hpp"
#include "ui/check.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using opensu::input::Action;
using opensu::input::Combo;
using opensu::input::Shortcuts;
using opensu::session::GameKeys;
using opensu::session::ShortcutChord;

int code(const char* name) {
    return opensu::test::need(opensu::input::comboNamed(name), "a key name").key;
}

const int leftShift = opensu::input::modifierKeys()[0];
const int rightShift = opensu::input::modifierKeys()[1];
const int leftControl = opensu::input::modifierKeys()[2];

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void chordEdges() {
    ShortcutChord chord;
    const Combo shiftTab{code("tab"), false, true};
    expect(chord.key(code("tab"), true) == Combo{code("tab")}, "Tab alone is a bare Tab");
    expect(!chord.key(code("tab"), false), "Tab release fires nothing");
    expect(!chord.key(leftShift, true), "Shift alone is no combination");
    expect(chord.key(code("tab"), true) == shiftTab, "Shift+Tab");
    expect(!chord.key(code("tab"), true), "a held Tab's repeat fires nothing");
    expect(!chord.key(code("tab"), false), "releasing Tab fires nothing");
    expect(chord.key(code("tab"), true) == shiftTab, "a second tap fires again");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(leftShift, false), "Shift up");
    expect(chord.key(code("tab"), true) == Combo{code("tab")},
           "Tab after Shift is released is bare");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(rightShift, true), "right Shift down");
    expect(chord.key(code("tab"), true) == shiftTab, "right Shift+Tab");
    expect(!chord.key(code("tab"), false), "Tab up");
    expect(!chord.key(rightShift, false), "Shift up");
    expect(!chord.key(leftControl, true), "Ctrl down");
    expect(chord.key(code("up"), true) == Combo{code("up"), true, false}, "Ctrl+Up");
}

/// An Xvfb on a display it picks itself, ended on scope exit.
class Xvfb {
  public:
    Xvfb() {
        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            return;
        }
        pid_ = fork();
        if (pid_ == 0) {
            close(pipeFds[0]);
            const std::string fd = std::to_string(pipeFds[1]);
            execlp("Xvfb", "Xvfb", "-displayfd", fd.c_str(), "-nolisten", "tcp", nullptr);
            _exit(127);
        }
        close(pipeFds[1]);
        std::string number;
        char c = 0;
        while (read(pipeFds[0], &c, 1) == 1 && c != '\n') {
            number += c;
        }
        close(pipeFds[0]);
        if (!number.empty()) {
            display_ = ":" + number;
        }
    }
    ~Xvfb() {
        if (pid_ > 0) {
            kill(pid_, SIGTERM);
            waitpid(pid_, nullptr, 0);
        }
    }
    Xvfb(const Xvfb&) = delete;
    Xvfb& operator=(const Xvfb&) = delete;

    [[nodiscard]] const std::optional<std::string>& display() const {
        return display_;
    }

  private:
    pid_t pid_{-1};
    std::optional<std::string> display_;
};

std::vector<Action> pollFor(GameKeys& keys, const Shortcuts& shortcuts, std::size_t wanted) {
    std::vector<Action> seen;
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (std::chrono::steady_clock::now() < end) {
        for (Action shortcut : keys.poll(shortcuts)) {
            seen.push_back(shortcut);
        }
        if (seen.size() >= wanted) {
            // Anything late would be a second, wrong Guide.
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            for (Action shortcut : keys.poll(shortcuts)) {
                seen.push_back(shortcut);
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return seen;
}

void tap(Display* display, KeySym modifier, KeySym key) {
    const KeyCode modifierCode = modifier == NoSymbol ? 0 : XKeysymToKeycode(display, modifier);
    const KeyCode keyCode = XKeysymToKeycode(display, key);
    if (modifierCode != 0) {
        XTestFakeKeyEvent(display, modifierCode, True, CurrentTime);
    }
    XTestFakeKeyEvent(display, keyCode, True, CurrentTime);
    XTestFakeKeyEvent(display, keyCode, False, CurrentTime);
    if (modifierCode != 0) {
        XTestFakeKeyEvent(display, modifierCode, False, CurrentTime);
    }
    XFlush(display);
}

int rawKeysFromAServer() {
    const Xvfb server;
    if (!server.display()) {
        std::fprintf(stderr, "SKIP: Xvfb did not start\n");
        return 77;
    }
    setenv("DISPLAY", server.display()->c_str(), 1);
    GameKeys keys;
    const Shortcuts shortcuts;
    Display* injector = XOpenDisplay(nullptr);
    expect(injector != nullptr, "the injector connects");

    tap(injector, NoSymbol, XK_Tab);
    tap(injector, XK_Shift_L, XK_a);
    tap(injector, XK_Control_L, XK_f);
    expect(pollFor(keys, shortcuts, 1).empty(), "Tab, Shift+A and Ctrl+F are no game shortcut");

    tap(injector, XK_Shift_L, XK_Tab);
    expect(pollFor(keys, shortcuts, 1) == std::vector{Action::Guide}, "Shift+Tab is one Guide");

    tap(injector, XK_Shift_R, XK_Tab);
    expect(pollFor(keys, shortcuts, 1) == std::vector{Action::Guide},
           "right Shift+Tab is one Guide");

    tap(injector, XK_Control_L, XK_Up);
    tap(injector, XK_Control_R, XK_m);
    expect(pollFor(keys, shortcuts, 2) == (std::vector{Action::VolumeUp, Action::VolumeMute}),
           "Ctrl+Up and Ctrl+M are the volume");

    Shortcuts remapped;
    expect(remapped.rebind(Action::Guide, Combo{code("f9")}).empty(), "a remap");
    tap(injector, XK_Shift_L, XK_Tab);
    tap(injector, NoSymbol, XK_F9);
    expect(pollFor(keys, remapped, 1) == std::vector{Action::Guide},
           "a remapped Guide follows the table, and the old chord no longer fires");

    XCloseDisplay(injector);
    return 0;
}

} // namespace

int main() {
    chordEdges();
    return rawKeysFromAServer();
}

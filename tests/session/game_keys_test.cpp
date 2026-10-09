// Shift+Tab as Guide: the chord's edges, and the raw key path against a real X server (Xvfb,
// keys injected with XTest).
#include "session/game_keys.hpp"

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

using opensu::session::GameKeys;
using opensu::session::GameShortcut;
using opensu::session::ShortcutChord;
using opensu::session::ShortcutKey;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void chordEdges() {
    ShortcutChord chord;
    expect(!chord.key(ShortcutKey::Tab, true), "Tab alone is no shortcut");
    expect(!chord.key(ShortcutKey::Tab, false), "Tab release alone is no shortcut");
    expect(!chord.key(ShortcutKey::LeftShift, true), "Shift alone is no shortcut");
    expect(chord.key(ShortcutKey::Tab, true) == GameShortcut::Guide, "Shift+Tab is Guide");
    expect(!chord.key(ShortcutKey::Tab, true), "a held Tab's repeat fires nothing");
    expect(!chord.key(ShortcutKey::Tab, false), "releasing Tab fires nothing");
    expect(chord.key(ShortcutKey::Tab, true) == GameShortcut::Guide, "a second tap is Guide again");
    expect(!chord.key(ShortcutKey::Tab, false), "Tab up");
    expect(!chord.key(ShortcutKey::LeftShift, false), "Shift up");
    expect(!chord.key(ShortcutKey::Tab, true), "Tab after Shift is released is no shortcut");
    expect(!chord.key(ShortcutKey::Tab, false), "Tab up");
    expect(!chord.key(ShortcutKey::RightShift, true), "right Shift down");
    expect(!chord.key(ShortcutKey::Other, true), "another key changes nothing");
    expect(chord.key(ShortcutKey::Tab, true) == GameShortcut::Guide, "right Shift+Tab is Guide");
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

std::vector<GameShortcut> pollFor(GameKeys& keys, std::size_t wanted) {
    std::vector<GameShortcut> seen;
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (std::chrono::steady_clock::now() < end) {
        for (GameShortcut shortcut : keys.poll()) {
            seen.push_back(shortcut);
        }
        if (seen.size() >= wanted) {
            // Anything late would be a second, wrong Guide.
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            for (GameShortcut shortcut : keys.poll()) {
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
    Display* injector = XOpenDisplay(nullptr);
    expect(injector != nullptr, "the injector connects");

    tap(injector, NoSymbol, XK_Tab);
    tap(injector, XK_Shift_L, XK_a);
    expect(pollFor(keys, 1).empty(), "Tab alone and Shift+A are no shortcut");

    tap(injector, XK_Shift_L, XK_Tab);
    expect(pollFor(keys, 1) == std::vector{GameShortcut::Guide}, "Shift+Tab is one Guide");

    tap(injector, XK_Shift_R, XK_Tab);
    expect(pollFor(keys, 1) == std::vector{GameShortcut::Guide}, "right Shift+Tab is one Guide");

    XCloseDisplay(injector);
    return 0;
}

} // namespace

int main() {
    chordEdges();
    return rawKeysFromAServer();
}

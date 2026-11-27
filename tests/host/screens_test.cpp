// The screens through a fake kscreen-doctor.
#include "host/screens.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::host::Runner;
using opensu::host::Screen;
using opensu::launch::Captured;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

// The Ally docked to a TV, the TV on and the panel off.
constexpr const char* docked = R"({"outputs": [
  {"id": 1, "name": "DP-2", "type": 14, "connected": true, "enabled": true, "priority": 1,
   "currentModeId": "12", "modes": [{"id": "1", "refreshRate": 30, "size": {"width": 3840, "height": 2160}},
   {"id": "12", "refreshRate": 60.0, "size": {"width": 1920, "height": 1080}}]},
  {"id": 2, "name": "eDP-1", "type": 7, "connected": true, "enabled": false, "priority": 1,
   "currentModeId": "53", "modes": [{"id": "53", "refreshRate": 119.999, "size": {"width": 1920, "height": 1080}}]},
  {"id": 3, "name": "HDMI-A-1", "type": 12, "connected": false, "enabled": false, "priority": 0,
   "currentModeId": "", "modes": []}
]})";

void theAnswerIsRead() {
    const auto screens = opensu::host::screensOf(docked);
    expect(screens && screens->size() == 2, "the connected screens");
    expect((*screens)[0] == Screen{"eDP-1", "Built-in screen", "1920x1080 at 120 Hz", false},
           "the built-in one first, off");
    expect((*screens)[1] == Screen{"DP-2", "DisplayPort (DP-2)", "1920x1080 at 60 Hz", true},
           "the TV in use, in the mode it is in");
    expect(!opensu::host::screensOf("not json") && !opensu::host::screensOf("{}"),
           "anything else is no answer");
}

void usingAScreenTurnsTheOthersOff() {
    std::vector<std::vector<std::string>> calls;
    const Runner run = [&calls](const std::string& program,
                                const std::vector<std::string>& args) -> std::optional<Captured> {
        expect(program == "env" && args.size() >= 3 && args[0] == "WAYLAND_DISPLAY=wayland-1" &&
                   args[1] == "QT_QPA_PLATFORM=wayland" && args[2] == "kscreen-doctor",
               "kscreen-doctor runs on the desktop's socket, as a Wayland client");
        calls.emplace_back(args.begin() + 3, args.end());
        return Captured{0, calls.back() == std::vector<std::string>{"-j"} ? docked : ""};
    };
    const auto screens = opensu::host::makeKScreenScreens(run, "wayland-1");
    expect(screens->use("eDP-1").empty(), "switching succeeds");
    expect(calls.back() == std::vector<std::string>{"output.eDP-1.enable",
                                                    "output.eDP-1.priority.1",
                                                    "output.DP-2.disable"},
           "the picked screen on and main, the other off, its mode left as it was");
    expect(screens->use("HDMI-A-1") == "HDMI-A-1 is not connected", "a screen not there");
    const auto readOnly =
        opensu::host::makeReadOnlyScreens(opensu::host::makeKScreenScreens(run, "wayland-1"), "no");
    const std::size_t before = calls.size();
    expect(readOnly->read() && readOnly->use("eDP-1") == "no" && calls.size() == before + 1,
           "a hidden run reads the screens and switches none");
    expect(!opensu::host::makeKScreenScreens(run, "")->read() && calls.size() == before + 1,
           "without a desktop there are no screens to choose, and nothing runs");
}

} // namespace

int main() try {
    theAnswerIsRead();
    usingAScreenTurnsTheOthersOff();
    std::printf("screens: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

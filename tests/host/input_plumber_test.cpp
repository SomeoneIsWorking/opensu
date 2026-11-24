// InputPlumber's Steam-only pads made readable, against a fake busctl.
#include "host/input_plumber.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::host::Runner;
using opensu::launch::Captured;
using Targets = std::vector<std::string>;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void mapping() {
    using opensu::host::readableTargets;
    expect(readableTargets({"deck-uhid", "keyboard", "mouse"}) ==
               Targets{"xb360", "keyboard", "mouse"},
           "a Steam Deck HID pad becomes an Xbox 360 pad");
    expect(readableTargets({"deck", "xb360"}) == Targets{"xb360"},
           "a pad already there is not doubled");
    expect(!readableTargets({"xbox-elite", "keyboard"}),
           "targets openSU already reads are left alone");
    expect(!readableTargets({}), "no targets, nothing to change");
}

/// A fake InputPlumber with one composite device whose targets are `targets`.
struct Plumber {
    Targets targets;
    std::vector<Targets> sets;

    Runner runner() {
        return [this](const std::string& program,
                      const std::vector<std::string>& args) -> std::optional<Captured> {
            expect(program == "busctl", "only busctl is run");
            if (args[1] == "--list") {
                return Captured{0, "/org/shadowblip/InputPlumber\n"
                                   "/org/shadowblip/InputPlumber/CompositeDevice0\n"
                                   "/org/shadowblip/InputPlumber/devices/target/gamepad0\n"};
            }
            if (args[2] == "get-property" && args[6] == "TargetDevices") {
                std::string data;
                for (std::size_t i = 0; i < targets.size(); ++i) {
                    data += (i == 0 ? "\"" : ",\"") + std::string{"/t/"} + std::to_string(i) + "\"";
                }
                return Captured{0, R"({"type":"as","data":[)" + data + "]}"};
            }
            if (args[2] == "get-property" && args[6] == "DeviceType") {
                const std::size_t index = std::stoul(args[4].substr(3));
                return Captured{0, R"({"type":"s","data":")" + targets[index] + "\"}"};
            }
            expect(args[5] == "SetTargetDevices", "the only call sets the targets");
            expect(args[3] == "/org/shadowblip/InputPlumber/CompositeDevice0",
                   "on the composite device");
            sets.emplace_back(args.begin() + 8, args.end());
            return Captured{0, ""};
        };
    }
};

void borrowsAndRestores() {
    Plumber plumber{.targets = {"deck-uhid", "keyboard", "mouse"}, .sets = {}};
    {
        const auto pads = opensu::host::makeReadablePads(plumber.runner());
        expect(plumber.sets.size() == 1 && plumber.sets[0] == Targets{"xb360", "keyboard", "mouse"},
               "the Steam Deck pad is an Xbox 360 pad while openSU runs");
    }
    expect(plumber.sets.size() == 2 && plumber.sets[1] == Targets{"deck-uhid", "keyboard", "mouse"},
           "the targets it found are put back as openSU ends");
}

void leavesReadablePadsAlone() {
    Plumber plumber{.targets = {"xb360", "keyboard"}, .sets = {}};
    {
        const auto pads = opensu::host::makeReadablePads(plumber.runner());
    }
    expect(plumber.sets.empty(), "a pad openSU reads is never touched");
}

void withoutInputPlumber() {
    const auto pads = opensu::host::makeReadablePads(
        [](const std::string&, const std::vector<std::string>&) -> std::optional<Captured> {
            return Captured{1, "The name is not activatable"};
        });
    expect(pads != nullptr, "a machine without InputPlumber still gets its holder");
}

} // namespace

int main() try {
    mapping();
    borrowsAndRestores();
    leavesReadablePadsAlone();
    withoutInputPlumber();
    std::puts("input_plumber: all checks passed");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

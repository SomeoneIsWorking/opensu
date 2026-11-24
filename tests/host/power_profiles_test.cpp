// PowerStation's power profiles, against a fake busctl.
#include "host/power_profiles.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using opensu::host::Runner;
using opensu::launch::Captured;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

constexpr const char* card = "/org/shadowblip/Performance/GPU/card1";
constexpr const char* properties =
    R"({"type":"a{sv}","data":[{"MaxTdp":{"type":"d","data":3.0e+01},)"
    R"("PowerProfilesAvailable":{"type":"as","data":["quiet","balanced","performance"]},)"
    R"("PowerProfile":{"type":"s","data":"balanced"},"TDP":{"type":"d","data":1.5e+01},)"
    R"("MinTdp":{"type":"d","data":5.0e+00}}]})";

struct Station {
    std::vector<std::vector<std::string>> sets;

    Runner runner() {
        return [this](const std::string&,
                      const std::vector<std::string>& args) -> std::optional<Captured> {
            if (args[1] == "--list") {
                return Captured{0, "/org/shadowblip/Performance\n"
                                   "/org/shadowblip/Performance/GPU/card0\n"
                                   "/org/shadowblip/Performance/GPU/card1\n"
                                   "/org/shadowblip/Performance/GPU/card1/HDMI-A-1\n"};
            }
            if (args[2] == "call") {
                return args[4] == card ? Captured{0, properties} : Captured{1, "Unknown interface"};
            }
            sets.push_back(args);
            return Captured{0, ""};
        };
    }
};

std::vector<std::string> property(const char* name, const char* signature, const char* value) {
    return {"--system",
            "set-property",
            "org.shadowblip.PowerStation",
            card,
            "org.shadowblip.GPU.Card.TDP",
            name,
            signature,
            value};
}

void readsAndSets() {
    Station station;
    const auto profiles = opensu::host::makePowerStationProfiles(station.runner());
    expect(profiles != nullptr, "the card with a TDP interface is found");
    const auto state = profiles->read();
    expect(state && state->profile == "balanced" && state->profiles.size() == 3,
           "the profile and the ones to choose from are read");
    expect(profiles->set("quiet").empty(), "a profile is set");
    expect(station.sets ==
               std::vector<std::vector<std::string>>{property("PowerProfile", "s", "quiet"),
                                                     property("TDP", "d", "10")},
           "quiet sets PowerStation's profile and a third of the most power");
    station.sets.clear();
    expect(profiles->set("performance").empty() &&
               station.sets.back() == property("TDP", "d", "30"),
           "performance gives all of it");
}

void wattsStayInRange() {
    expect(opensu::host::profileWatts("balanced", 5, 30) == 15, "balanced takes half");
    expect(opensu::host::profileWatts("quiet", 8, 15) == 8, "never below the least");
}

void hiddenRunsOnlyRead() {
    Station station;
    const auto profiles = opensu::host::makeReadOnlyPowerProfiles(
        opensu::host::makePowerStationProfiles(station.runner()), "changes are off");
    expect(profiles->read().has_value(), "a hidden run reads the profile");
    expect(profiles->set("quiet") == "changes are off", "and refuses to change it");
    expect(station.sets.empty(), "nothing is written");
}

void withoutPowerStation() {
    const auto profiles = opensu::host::makePowerStationProfiles(
        [](const std::string&, const std::vector<std::string>&) -> std::optional<Captured> {
            return Captured{1, "The name is not activatable"};
        });
    expect(profiles == nullptr, "no PowerStation, no power profile");
}

} // namespace

int main() try {
    readsAndSets();
    wattsStayInRange();
    hiddenRunsOnlyRead();
    withoutPowerStation();
    std::puts("power_profiles: all checks passed");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// The stick lights' rows and writes: through the kernel's files where they carry a colour, and
// where they cannot (the Ally's capped zones) off only until the helper is installed, then every
// colour through it with the brightness folded in.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "ring_light_settings.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::string contents(const fs::path& file) {
    std::ifstream in{file};
    return {std::istreambuf_iterator<char>{in}, {}};
}

class FakeHelper final : public host::RingLightHelper {
  public:
    bool installed() const override {
        return isInstalled;
    }
    std::string installBlocker() const override {
        return {};
    }
    host::RootStep stage() override {
        return {};
    }
    host::InstallResult install() override {
        isInstalled = true;
        return {};
    }
    std::string set(std::uint32_t rgb) override {
        sent.push_back(rgb);
        return {};
    }

    bool isInstalled{false};
    std::vector<std::uint32_t> sent;
};

struct Rig {
    explicit Rig(const char* name, const char* zoneMax)
        : root{fs::path{OPENSU_TEST_SCRATCH} / "ring_light_settings" / name},
          led{root / "leds" / "ally:rgb:joystick_rings"},
          preferences{(fs::remove_all(root), fs::create_directories(led),
                       settings::Store{root / "settings.json"}),
                      [](const std::string&) {}} {
        std::ofstream{led / "multi_index"} << "rgb rgb rgb rgb\n";
        std::ofstream{led / "max_brightness"} << "255\n";
        std::ofstream{led / "multi_intensity"} << "128 128 128 128\n";
        std::ofstream{led / "brightness"} << "128\n";
        if (zoneMax != nullptr) {
            std::ofstream{led / "multi_max_intensity"} << zoneMax << '\n';
        }
    }

    app::RingLightSettings settings(host::RingLightHelper* helper) {
        return app::RingLightSettings{preferences, device::RingLights::find(root / "leds"), helper,
                                      {.refused = [](const std::string&) {},
                                       .allowColours = [this] { ++allows; }}};
    }

    fs::path root;
    fs::path led;
    app::Preferences preferences;
    int allows{0};
};

const ui::SettingsRow* rowOf(const std::vector<ui::SettingsRow>& rows, const std::string& id) {
    for (const ui::SettingsRow& row : rows) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

void filesThatCarryAColourAreWrittenDirectly() {
    Rig rig{"direct", nullptr};
    FakeHelper helper;
    app::RingLightSettings lights = rig.settings(&helper);
    const std::vector<ui::SettingsRow> rows = lights.rows();
    expect(rowOf(rows, "lights.colour")->choices.size() == 11, "leave, off and nine colours");
    expect(rowOf(rows, "lights.allow") == nullptr, "no helper is asked for");
    lights.run("lights.colour", 4);
    expect(contents(rig.led / "multi_intensity") == "16736256 16736256 16736256 16736256",
           "Orange goes to the files");
    expect(helper.sent.empty(), "and never to the helper");
}

void cappedZonesOfferOffAndTheHelper() {
    Rig rig{"capped", "255 255 255 255"};
    FakeHelper helper;
    app::RingLightSettings lights = rig.settings(&helper);
    const std::vector<ui::SettingsRow> rows = lights.rows();
    expect(rowOf(rows, "lights.colour")->choices ==
               std::vector<std::string>{"As the system set them", "Off"},
           "without the helper only off is offered");
    expect(rowOf(rows, "lights.allow") != nullptr, "with a row to allow colours");
    lights.run("lights.allow", 0);
    expect(rig.allows == 1, "which starts the helper's install");
    lights.run("lights.colour", 1);
    expect(contents(rig.led / "brightness") == "0", "off goes to the files");
    expect(helper.sent.empty(), "the helper is not run before it is installed");
}

void theInstalledHelperCarriesEveryColour() {
    Rig rig{"helper", "255 255 255 255"};
    FakeHelper helper;
    helper.isInstalled = true;
    app::RingLightSettings lights = rig.settings(&helper);
    expect(rowOf(lights.rows(), "lights.allow") == nullptr, "nothing left to allow");
    expect(rowOf(lights.rows(), "lights.colour")->choices.size() == 11, "every colour is offered");
    lights.run("lights.colour", 4);
    lights.setLevel("lights.brightness", 50);
    expect(helper.sent == std::vector<std::uint32_t>{0xFF6000, 0x7F3000},
           "Orange, then at half brightness, folded into the colour");
    expect(contents(rig.led / "multi_intensity") == "128 128 128 128\n",
           "the kernel's files are left alone");
    lights.run("lights.colour", -3);
    expect(helper.sent.back() == 0, "Off is black through the helper");
}

} // namespace

int main() try {
    filesThatCarryAColourAreWrittenDirectly();
    cappedZonesOfferOffAndTheHelper();
    theInstalledHelperCarriesEveryColour();
    std::printf("ring_light_settings: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

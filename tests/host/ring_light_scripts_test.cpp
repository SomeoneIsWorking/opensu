// The stick lights' root-run scripts, run for real against a temp root (OPENSU_SELECT_ROOT,
// honoured only for a non-root caller): the installer staged as openSU stages it, its helper and
// sudoers rule checked by the real visudo, and the helper's strict colour and the five reports it
// writes to the Ally's configuration interface and nowhere else.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <pwd.h>
#include <unistd.h>

#include "host/ring_light_helper.hpp"
#include "launch/command.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

constexpr int skipped = 77;
constexpr std::size_t reportSize = 64;

std::string slurp(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

struct Run {
    int status{-1};
    std::string output;
};

Run runSh(const std::vector<std::string>& argv) {
    const auto out = launch::runCaptured("/bin/sh", argv, launch::CaptureErrors::Merged);
    return out ? Run{out->status, out->output} : Run{};
}

fs::path freshRoot(const std::string& name) {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "ring-light-scripts" / name;
    fs::remove_all(root);
    fs::create_directories(root);
    setenv("OPENSU_SELECT_ROOT", root.c_str(), 1);
    return root;
}

/// A hidraw node of ASUS's 0b05:1abe with `descriptor` as its report descriptor.
void hidraw(const fs::path& root, int number, const std::string& descriptor) {
    const fs::path dir = root / "sys/class/hidraw" / ("hidraw" + std::to_string(number));
    write(dir / "device/uevent", "DRIVER=asus_rog_ally\nHID_ID=0003:00000B05:00001ABE\n");
    write(dir / "device/report_descriptor", descriptor);
    write(root / "dev" / ("hidraw" + std::to_string(number)), "");
}

std::string report(std::initializer_list<unsigned char> bytes) {
    std::string out(reportSize, '\0');
    std::size_t at = 0;
    for (const unsigned char byte : bytes) {
        out[at++] = static_cast<char>(byte);
    }
    return out;
}

void theInstallerPlacesTheHelperAndAValidSudoersRule() {
    const fs::path root = freshRoot("installer");
    host::RingLightHelperPaths paths;
    paths.stage = root / "run" / "ring-lights-install";
    paths.pkexec = "/usr/bin/pkexec";
    const host::RootStep step = host::stageRootStep(host::ringLightInstallPlan(paths));
    expect(step.shown() ==
               "/usr/bin/pkexec /bin/sh " + (paths.stage / "install-ring-lights.sh").string(),
           "the root step is pkexec running the staged installer through /bin/sh");
    expect(fs::is_regular_file(paths.stage / "root-install.sh"),
           "the shared steps are staged beside it");

    const std::string user = getpwuid(getuid())->pw_name;
    setenv("PKEXEC_UID", std::to_string(getuid()).c_str(), 1);
    const Run done = runSh({step.args.back()});
    expect(done.status == 0 && done.output == "opensu-install: start\nopensu-install: done\n",
           "the installer succeeds between the marker and done");
    const fs::path helper = root / "usr/local/libexec/opensu-ring-lights";
    expect(slurp(helper) == slurp(fs::path{OPENSU_PACKAGING_DIR} / "opensu-ring-lights.sh"),
           "the helper built into openSU is installed");
    const fs::path rule = root / "etc/sudoers.d/zz-opensu-ring-lights";
    expect(slurp(rule) == "# Installed by openSU: " + user + " may set the stick lights' colour.\n" +
                              user + " ALL=(root) NOPASSWD: /usr/local/libexec/opensu-ring-lights\n",
           "the rule lets pkexec's user run the helper and nothing else");
    const auto checked = launch::runCaptured("/usr/sbin/visudo", {"-cf", rule.string()},
                                             launch::CaptureErrors::Merged);
    expect(checked && checked->status == 0, "the real visudo accepts the rule");
    setenv("PKEXEC_UID", "0", 1);
    expect(runSh({step.args.back()}).status == 1, "root as the user asking is refused");
}

void theHelperWritesFiveReportsToTheConfigurationInterface() {
    const fs::path root = freshRoot("helper");
    // A keyboard interface first, then the configuration one: vendor page 0xff31, report 0x5a.
    hidraw(root, 1, std::string{"\x05\x01\x09\x06\xa1\x01\x85\x01", 8});
    hidraw(root, 3, std::string{"\x06\x31\xff\x09\x76\xa1\x01\x85\x5a\xc0", 10});
    const std::string script = (fs::path{OPENSU_PACKAGING_DIR} / "opensu-ring-lights.sh").string();
    const Run done = runSh({script, "ff6000"});
    expect(done.status == 0, "a colour is set");
    expect(slurp(root / "dev/hidraw1").empty(), "the keyboard interface is left alone");
    expect(slurp(root / "dev/hidraw3") ==
               report({0x5a, 'A', 'S', 'U', 'S', ' ', 'T', 'e', 'c', 'h', '.', 'I', 'n', 'c', '.'}) +
                   report({0x5a, 0xba, 0xc5, 0xc4, 0x03}) +
                   report({0x5a, 0xb3, 0x00, 0x00, 0xff, 0x60, 0x00}) + report({0x5a, 0xb5}) +
                   report({0x5a, 0xb4}),
           "start, full brightness, every zone Orange, set, apply");
    write(root / "dev/hidraw3", "");
    expect(runSh({script, "000000"}).status == 0 &&
               slurp(root / "dev/hidraw3").substr(reportSize, reportSize) ==
                   report({0x5a, 0xba, 0xc5, 0xc4, 0x00}),
           "black turns the brightness off");
    write(root / "dev/hidraw3", "");
    expect(runSh({script, "rainbow", "2"}).status == 0 &&
               slurp(root / "dev/hidraw3").substr(reportSize, 3 * reportSize) ==
                   report({0x5a, 0xba, 0xc5, 0xc4, 0x02}) +
                       report({0x5a, 0xb3, 0x00, 0x03, 0x00, 0x00, 0x00, 0xeb}) +
                       report({0x5a, 0xb5}),
           "rainbow 2 sets level 2 and the rainbow at medium speed");
    for (const char* level : {"0", "4", "x"}) {
        expect(runSh({script, "rainbow", level}).status == 1, "the rainbow's level is 1 to 3");
    }
    for (const char* bad : {"FF6000", "ff600", "ff60000", "../x", "", "rainbow"}) {
        expect(runSh({script, bad}).status == 1, "anything but six lower-case hex digits is refused");
    }
    expect(runSh({script, "ff6000", "x"}).status == 1, "and so is a second argument");
    fs::remove_all(root / "sys/class/hidraw/hidraw3");
    const Run none = runSh({script, "ff6000"});
    expect(none.status == 1 && none.output.find("no ROG Ally controller") != std::string::npos,
           "without the Ally's interface it says so");
}

} // namespace

int main() try {
    if (geteuid() == 0) {
        std::fprintf(stderr, "SKIP: the scripts honour the test root only for a non-root caller\n");
        return skipped;
    }
    theInstallerPlacesTheHelperAndAValidSudoersRule();
    theHelperWritesFiveReportsToTheConfigurationInterface();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

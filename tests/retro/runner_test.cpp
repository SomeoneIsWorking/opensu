// opensu-retro end to end: the hidden runner against the fake core draws its bars letterboxed,
// writes and reloads save RAM, refuses what it cannot load, and dies alone when its core crashes.
// Its control channel announces itself, saves, loads and resets between frames, refuses a core
// without states and a missing state, and leaves a load failure for the shell.
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <libretro.h>
#include <nlohmann/json.hpp>
#include <png.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.hpp"
#include "lucent/http_client.h"
#include "scratch_dir.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;

struct Status {
    bool exited{false};
    int code{0};
    int signal{0};
};

/// Starts the runner with OPENSU_RETRO_CONTROL set to `control`, or unset when it is empty.
pid_t spawn(const std::vector<std::string>& args, const std::filesystem::path& control = {}) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    const pid_t child = fork();
    if (child == 0) {
        // The crashing core must not leave a core dump behind.
        const rlimit none{0, 0};
        setrlimit(RLIMIT_CORE, &none);
        if (control.empty()) {
            unsetenv("OPENSU_RETRO_CONTROL");
        } else {
            setenv("OPENSU_RETRO_CONTROL", control.c_str(), 1);
        }
        execv(argv[0], argv.data());
        _exit(127);
    }
    return child;
}

Status wait(pid_t child) {
    int status = 0;
    waitpid(child, &status, 0);
    if (WIFSIGNALED(status)) {
        return {false, 0, WTERMSIG(status)};
    }
    return {true, WEXITSTATUS(status), 0};
}

Status run(const std::vector<std::string>& args, const std::filesystem::path& control = {}) {
    return wait(spawn(args, control));
}

struct Image {
    std::vector<unsigned char> rgba;
    unsigned width{0};
    unsigned height{0};

    [[nodiscard]] const unsigned char* at(unsigned x, unsigned y) const {
        return rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
    }
};

Image readPng(const std::filesystem::path& file) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_file(&image, file.c_str()) == 0) {
        fail("the frame PNG reads");
    }
    image.format = PNG_FORMAT_RGBA;
    Image out;
    out.width = image.width;
    out.height = image.height;
    out.rgba.resize(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, out.rgba.data(), 0, nullptr) == 0) {
        fail("the frame PNG decodes");
    }
    return out;
}

std::string read(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void expectColour(const Image& image, unsigned x, unsigned y, unsigned char r, unsigned char g,
                  unsigned char b, const char* what) {
    const unsigned char* pixel = image.at(x, y);
    if (pixel[0] != r || pixel[1] != g || pixel[2] != b) {
        std::fprintf(stderr, "at %u,%u: %u %u %u\n", x, y, pixel[0], pixel[1], pixel[2]);
        fail(what);
    }
}

void checkFrame(const Image& image) {
    expect(image.width > 0 && image.height > 0, "the frame has a size");
    const double aspect = 4.0 / 3.0;
    double drawnWidth = image.width;
    double drawnHeight = image.height;
    if (static_cast<double>(image.width) / image.height > aspect) {
        drawnWidth = image.height * aspect;
    } else {
        drawnHeight = image.width / aspect;
    }
    const double left = (image.width - drawnWidth) / 2.0;
    const double top = (image.height - drawnHeight) / 2.0;
    const auto y = static_cast<unsigned>(top + drawnHeight / 2.0);
    const unsigned char bars[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
    for (int bar = 0; bar < 4; ++bar) {
        const auto x = static_cast<unsigned>(left + drawnWidth * (bar + 0.5) / 4.0);
        expectColour(image, x, y, bars[bar][0], bars[bar][1], bars[bar][2],
                     "the core's bars fill the letterboxed frame");
    }
    expect(left >= 2.0 || top >= 2.0, "the window's shape leaves a letterbox");
    if (left >= 2.0) {
        expectColour(image, static_cast<unsigned>(left / 2.0), y, 0, 0, 0,
                     "the bars at the sides are black");
    } else {
        expectColour(image, image.width / 2, static_cast<unsigned>(top / 2.0), 0, 0, 0,
                     "the bars above and below are black");
    }
}

nlohmann::json readJson(const std::filesystem::path& file) {
    return nlohmann::json::parse(read(file), nullptr, false);
}

/// The port the runner announced in `control`, waiting for it; fails if `child` exits first.
std::uint16_t awaitPort(const std::filesystem::path& control, pid_t child) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{30};
    while (std::chrono::steady_clock::now() < until) {
        const nlohmann::json announce = readJson(control);
        if (announce.is_object() && announce.contains("port") && announce.contains("pid")) {
            expect(announce["pid"] == child, "the control file names the runner's pid");
            return announce["port"].get<std::uint16_t>();
        }
        int status = 0;
        expect(waitpid(child, &status, WNOHANG) == 0, "the runner runs until it announces");
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    fail("the runner announced its port");
}

nlohmann::json status(std::uint16_t port) {
    lucent::http::FetchResult result;
    std::string error;
    expect(lucent::http::get_loopback(port, "/status", result, error) && result.status == 200,
           "GET /status answers");
    return nlohmann::json::parse(result.body);
}

lucent::http::FetchResult post(std::uint16_t port, const char* target,
                               const std::string& body = {}) {
    lucent::http::FetchResult result;
    std::string error;
    expect(lucent::http::post_loopback(port, target, body, result, error), "the POST is answered");
    return result;
}

/// Waits until the runner has run `frames` frames.
void awaitFrame(std::uint16_t port, std::uint64_t frames) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{30};
    while (status(port)["frame"].get<std::uint64_t>() < frames) {
        expect(std::chrono::steady_clock::now() < until, "the runner reaches the frame");
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

std::uint32_t countIn(const std::string& bytes) {
    std::uint32_t count = 0;
    expect(bytes.size() >= sizeof(count), "the file holds the frame count");
    std::memcpy(&count, bytes.data(), sizeof(count));
    return count;
}

struct Paths {
    std::string runner;
    std::string core;
    std::filesystem::path rom;
    std::filesystem::path root;
};

/// Save, a later load, and the frame count at exit showing the frames between them undone.
void stateRoundTrip(const Paths& paths) {
    const std::filesystem::path control = paths.root / "run" / "control.json";
    const std::filesystem::path saves = paths.root / "state-saves";
    const pid_t child = spawn({paths.runner, "--core", paths.core, "--saves", saves.string(),
                               "--hidden", "--frames", "600", paths.rom.string()},
                              control);
    const std::uint16_t port = awaitPort(control, child);
    nlohmann::json now = status(port);
    expect(now["core"] == "fake" && now["rom"] == paths.rom.string(), "/status names the game");
    expect(now["hasState"] == false, "no state before the first save");
    expect(post(port, "/state/load").status == 404, "loading a missing state is 404");
    awaitFrame(port, 10);
    expect(post(port, "/state/save").status == 200, "the state is saved");
    const std::string state = read(saves / "game.state");
    expect(state.size() == 256 + 8, "the slot holds the core's whole state");
    const std::uint32_t saved = countIn(state);
    expect(saved >= 10, "the state was taken between frames");
    expect(status(port)["hasState"] == true, "/status shows the state");
    awaitFrame(port, saved + 60);
    expect(post(port, "/state/load").status == 200, "the state is loaded");
    const Status ended = wait(child);
    expect(ended.exited && ended.code == 0, "the runner ends after its frames");
    expect(!std::filesystem::exists(control), "the control file goes at exit");
    const std::uint32_t count = countIn(read(saves / "game.srm"));
    expect(count > saved && count <= 600 - 60, "the load took the core back to the saved frame");
}

void resetRestarts(const Paths& paths) {
    const std::filesystem::path control = paths.root / "run" / "reset.json";
    const std::filesystem::path saves = paths.root / "reset-saves";
    const pid_t child = spawn({paths.runner, "--core", paths.core, "--saves", saves.string(),
                               "--hidden", "--frames", "300", paths.rom.string()},
                              control);
    const std::uint16_t port = awaitPort(control, child);
    awaitFrame(port, 100);
    expect(post(port, "/reset").status == 200, "the game is reset");
    const Status ended = wait(child);
    expect(ended.exited && ended.code == 0, "the runner ends after its frames");
    expect(countIn(read(saves / "game.srm")) <= 200, "the reset zeroed the core's frame count");
}

void statelessCoreRefuses(const Paths& paths, const std::string& stateless) {
    const std::filesystem::path control = paths.root / "run" / "stateless.json";
    const std::filesystem::path saves = paths.root / "stateless-saves";
    const pid_t child = spawn({paths.runner, "--core", stateless, "--saves", saves.string(),
                               "--hidden", "--frames", "1200", paths.rom.string()},
                              control);
    const std::uint16_t port = awaitPort(control, child);
    const lucent::http::FetchResult refused = post(port, "/state/save");
    expect(refused.status == 409 && refused.body == "fake cannot save states",
           "a core that cannot serialize is 409 with the reason");
    expect(!std::filesystem::exists(saves / "game.state"), "nothing is written for it");
    expect(post(port, "/nowhere").status == 404, "an unknown route is 404");
    expect(post(port, "/input", R"({"buttons": ["start"], "right": [0.5, -1]})").status == 200,
           "the control pad takes buttons and sticks");
    expect(post(port, "/input", R"({"buttons": ["guide"]})").status == 400,
           "Guide is not the control pad's");
    expect(post(port, "/input", R"({"left": [2, 0]})").status == 400,
           "a stick past its travel is refused");
    awaitFrame(port, status(port)["frame"].get<std::uint64_t>() + 5);
    kill(child, SIGTERM);
    const Status ended = wait(child);
    expect(ended.exited && ended.code == 0, "SIGTERM quits the runner cleanly");
    const std::string saved = read(saves / "game.srm");
    expect(saved.size() > 4 &&
               (static_cast<unsigned char>(saved[4]) & (1U << RETRO_DEVICE_ID_JOYPAD_START)) != 0,
           "the control pad's Start reached the core");
    expect(!std::filesystem::exists(control), "the control file goes at exit");
}

void loadFailureIsReported(const Paths& paths) {
    const std::filesystem::path control = paths.root / "run" / "failed.json";
    const Status failed =
        run({paths.runner, "--core", paths.core, "--saves", (paths.root / "saves").string(),
             "--hidden", "--frames", "1", (paths.root / "missing.bin").string()},
            control);
    expect(failed.exited && failed.code == 2, "a missing game exits 2");
    const nlohmann::json report = readJson(control);
    expect(report.is_object() && report.size() == 1 &&
               report["error"].get<std::string>().find("missing.bin") != std::string::npos,
           "the control file holds the load failure for the shell");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        fail("usage: runner_test <opensu-retro> <fake core> <crashing core> <stateless core>");
    }
    const std::string runner = argv[1];
    const std::string core = argv[2];
    const std::string crashing = argv[3];
    const std::string stateless = argv[4];
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "retro_runner"};
    const std::filesystem::path rom = scratch.path() / "game.bin";
    std::ofstream{rom, std::ios::binary} << "Bgame";
    const std::string saves = (scratch.path() / "saves").string();
    const std::string system = (scratch.path() / "system").string();
    const std::string png = (scratch.path() / "frame.png").string();
    const std::filesystem::path srm = scratch.path() / "saves" / "game.srm";

    const Status first =
        run({runner, "--core", core, "--saves", saves, "--system", system, "--hidden", "--validate",
             "--frames", "30", "--frame-png", png, rom.string()});
    expect(first.exited && first.code == 0, "the hidden runner exits 0 after its frames");
    checkFrame(readPng(png));
    std::string ram = read(srm);
    expect(ram.size() == 256, "the save RAM is written whole");
    expect(ram[0] == 30 && ram[1] == 0, "the save holds the frames the core ran");
    expect(ram[4] == 0 && ram[5] == 0, "a hidden run reads no input");
    expect(ram[6] == 'B' && static_cast<unsigned char>(ram[7]) == 0xA5,
           "the save holds what the core wrote at load");
    expect(std::filesystem::is_directory(system), "the system directory is made");

    const Status second = run({runner, "--core", core, "--saves", saves, "--system", system,
                               "--hidden", "--frames", "10", rom.string()});
    expect(second.exited && second.code == 0, "a second run exits 0");
    ram = read(srm);
    expect(ram[0] == 40, "the second run continued from the loaded save");

    const Status noRom = run({runner, "--core", core, "--saves", saves, "--hidden", "--frames", "1",
                              (scratch.path() / "missing.bin").string()});
    expect(noRom.exited && noRom.code == 2, "a missing game exits 2");
    const Status noCore = run({runner, "--core", (scratch.path() / "missing.so").string(),
                               "--saves", saves, "--hidden", "--frames", "1", rom.string()});
    expect(noCore.exited && noCore.code == 2, "a missing core exits 2");
    const Status badLine = run({runner, "--hidden", rom.string()});
    expect(badLine.exited && badLine.code == 1, "a command line without a core exits 1");

    const Status crashed = run(
        {runner, "--core", crashing, "--saves", saves, "--hidden", "--frames", "30", rom.string()});
    expect(!crashed.exited && crashed.signal == SIGSEGV,
           "a core crashing in retro_run kills the runner by its signal");
    expect(read(srm)[0] == 40, "the crash left the last save as it was");

    const Paths paths{runner, core, rom, scratch.path()};
    try {
        stateRoundTrip(paths);
        resetRestarts(paths);
        statelessCoreRefuses(paths, stateless);
        loadFailureIsReported(paths);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }

    std::printf("runner: all checks passed\n");
    return 0;
}

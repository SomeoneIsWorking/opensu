// opensu-retro end to end: the hidden runner against the fake core draws its bars letterboxed,
// writes and reloads save RAM, refuses what it cannot load, and dies alone when its core crashes.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <png.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "check.hpp"
#include "scratch_dir.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;

struct Status {
    bool exited{false};
    int code{0};
    int signal{0};
};

Status run(const std::vector<std::string>& args) {
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
        execv(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    if (WIFSIGNALED(status)) {
        return {false, 0, WTERMSIG(status)};
    }
    return {true, WEXITSTATUS(status), 0};
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

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        fail("usage: runner_test <opensu-retro> <fake core> <crashing core>");
    }
    const std::string runner = argv[1];
    const std::string core = argv[2];
    const std::string crashing = argv[3];
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

    std::printf("runner: all checks passed\n");
    return 0;
}

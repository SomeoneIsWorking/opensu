// A built-in core's system files through the shipping code, against a loopback host: the catalog's
// pin read, the folder unpacked under the system folder with its record, a player's file left, a new
// pin replacing and removing only openSU's files, a wrong SHA-256 refused with nothing written, and
// removal keeping the player's file.
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include "library/core_system_files.hpp"
#include "library/emulator_catalog.hpp"
#include "lucent/content.h"
#include "support/loopback_server.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "support/zip_fixture.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library::roms;
using opensu::test::expect;
using opensu::test::need;

constexpr auto anyProgress = [](std::uint64_t, std::uint64_t) {
    return true;
};

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void put(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

std::string sha256Of(const std::string& bytes) {
    return lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(std::span{bytes})));
}

/// Serves zips by path.
class ZipHost {
  public:
    void serve(const std::string& path, std::string zip) {
        const std::lock_guard lock{mutex_};
        files_[path] = std::move(zip);
    }
    [[nodiscard]] std::string url(const std::string& path) const {
        return server_.base() + path;
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::lock_guard lock{mutex_};
        const auto found = files_.find(std::string{request.path()});
        if (found == files_.end()) {
            return opensu::test::reply(404, "");
        }
        return lucent::http::Response::binary(200, "OK", "application/zip", found->second);
    }

    std::mutex mutex_;
    std::map<std::string, std::string> files_;
    opensu::test::LoopbackServer server_{[this](const lucent::http::Request& request) {
        return answer(request);
    }};
};

void catalogPinIsRead() {
    const std::string pinned = R"j([{"id": "d", "name": "D (built-in)", "systems": ["gc"],
        "core": "dolphin_libretro", "systemFiles": {"url": "https://h/a.zip", "sha256": ")j" +
                               std::string(64, 'a') +
                               R"j(", "folder": "dolphin-1/Data/Sys", "into": "dolphin-emu/Sys"}}])j";
    std::string error;
    const EmulatorCatalog catalog =
        need(EmulatorCatalog::parse(R"({"emulators": )" + pinned + "}", "/home/p", error),
             "a pinned core parses");
    expect(catalog.entries().front().systemFiles ==
               SystemFilesSource{"https://h/a.zip", std::string(64, 'a'), "dolphin-1/Data/Sys",
                                 "dolphin-emu/Sys"},
           "with its URL, SHA-256, folder and place");
    std::string escaping = pinned;
    escaping.replace(escaping.find("dolphin-emu/Sys"), 15, "../outside");
    expect(!EmulatorCatalog::parse(R"({"emulators": )" + escaping + "}", "/home/p", error),
           "a place outside the system folder is refused");
}

void installsReplacesAndRemoves(const fs::path& root) {
    ZipHost host;
    const std::string first = opensu::test::zipfixture::buildZip(
        {{"dolphin-1/README.md", "top", true, {}},
         {"dolphin-1/Data/Sys/codehandler.bin", "handler 1", true, {}},
         {"dolphin-1/Data/Sys/GC/font_western.bin", "font", true, {}},
         {"dolphin-1/Data/Sys/GC/USA/IPL.bin", "not the player's", true, {}},
         {"dolphin-1/Data/Sys/Wii/old.bin", "old", false, {}}});
    host.serve("/1.zip", first);
    const SystemFilesSource one{host.url("/1.zip"), sha256Of(first), "dolphin-1/Data/Sys",
                                "dolphin-emu/Sys"};
    const fs::path system = root / "system";
    const fs::path sys = system / "dolphin-emu" / "Sys";
    put(sys / "GC" / "USA" / "IPL.bin", "the player's BIOS");
    const CoreSystemFiles files{system, root / "cores"};
    const opensu::net::WebClient web;
    expect(!files.current("dolphin_libretro", one), "nothing is installed at first");

    const auto installed = files.install({}, web, "dolphin_libretro", one, anyProgress);
    expect(installed && *installed == 3, "the folder's three free files are written");
    expect(read(sys / "codehandler.bin") == "handler 1" && read(sys / "GC/font_western.bin") ==
                                                               "font" &&
               read(sys / "Wii/old.bin") == "old",
           "under the system folder's place, paths kept");
    expect(!fs::exists(system / "dolphin-emu" / "README.md"), "nothing outside the folder");
    expect(read(sys / "GC/USA/IPL.bin") == "the player's BIOS", "a player's file is left");
    expect(files.current("dolphin_libretro", one), "the record names the pin");

    const std::string second = opensu::test::zipfixture::buildZip(
        {{"dolphin-2/Data/Sys/codehandler.bin", "handler 2", true, {}},
         {"dolphin-2/Data/Sys/GC/font_western.bin", "font", true, {}},
         {"dolphin-2/Data/Sys/GC/USA/IPL.bin", "not the player's", true, {}}});
    host.serve("/2.zip", second);
    const SystemFilesSource two{host.url("/2.zip"), sha256Of(second), "dolphin-2/Data/Sys",
                                "dolphin-emu/Sys"};
    const SystemFilesSource wrong{two.url, std::string(64, '0'), two.folder, two.into};
    const auto refused = files.install({}, web, "dolphin_libretro", wrong, anyProgress);
    expect(!refused && refused.error().find("SHA-256") != std::string::npos &&
               read(sys / "codehandler.bin") == "handler 1" &&
               files.current("dolphin_libretro", one),
           "a zip unlike its pin is refused and nothing changes");
    expect(!fs::exists(root / "cores" / "dolphin_libretro.system.zip.part"),
           "and its download is not kept");

    expect(files.install({}, web, "dolphin_libretro", two, anyProgress).ok(),
           "a new pin installs");
    expect(read(sys / "codehandler.bin") == "handler 2", "its changed file is written");
    expect(!fs::exists(sys / "Wii"), "a file the new pin lacks goes, with its empty folder");
    expect(read(sys / "GC/USA/IPL.bin") == "the player's BIOS", "the player's file stays");
    expect(files.current("dolphin_libretro", two) && !files.current("dolphin_libretro", one),
           "the record names the new pin");

    std::string error;
    expect(files.remove("dolphin_libretro", error), "removal succeeds");
    expect(!fs::exists(sys / "codehandler.bin") && !fs::exists(sys / "GC/font_western.bin") &&
               read(sys / "GC/USA/IPL.bin") == "the player's BIOS",
           "removal takes openSU's files and leaves the player's");
    expect(!files.installed("dolphin_libretro"), "and the record");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "core_system_files"};
    catalogPinIsRead();
    installsReplacesAndRemoves(scratch.path());
    std::puts("core_system_files: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

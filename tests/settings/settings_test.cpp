// The settings file: defaults, a round trip, and what a damaged file does.
#include "settings/settings.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::LibraryMode;
using opensu::settings::Settings;
using opensu::settings::Store;
using opensu::test::expect;

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void defaults(const fs::path& root) {
    const Store store{root / "none" / "settings.json"};
    expect(store.load() == Settings{}, "no file gives the defaults");
    expect(store.load().libraryMode == LibraryMode::Standard, "Library starts as Standard");
}

void roundTrip(const fs::path& root) {
    const Store store{root / "nested" / "dir" / "settings.json"};
    for (const LibraryMode mode : opensu::library::allLibraryModes) {
        std::string error;
        expect(store.save(Settings{mode}, error), "a save makes its directories and writes");
        expect(store.load().libraryMode == mode, "what was saved is read back");
    }
    expect(!fs::exists(fs::path{store.file()}.concat(".part")),
           "the staging file is renamed away, not left");
    expect(read(store.file()).find("\"libraryMode\": \"carousel\"") != std::string::npos,
           "the file is JSON with the mode's key");
}

void damaged(const fs::path& root) {
    const fs::path file = root / "damaged" / "settings.json";
    const Store store{file};
    write(file, "{ not json");
    expect(store.load() == Settings{}, "text that is not JSON gives the defaults");
    write(file, "[1, 2]");
    expect(store.load() == Settings{}, "JSON that is not an object gives the defaults");
    write(file, R"({"libraryMode": "list"})");
    expect(store.load() == Settings{}, "a layout opensu has no card for gives the default");
    write(file, R"({"libraryMode": 3})");
    expect(store.load() == Settings{}, "a mode that is not a string gives the default");
    write(file, R"({"libraryMode": "xmb", "other": true})");
    expect(store.load().libraryMode == LibraryMode::Xmb, "a key opensu does not know is ignored");
}

void unwritable(const fs::path& root) {
    const fs::path blocker = root / "blocker";
    write(blocker, "a file where a directory should be");
    const Store store{blocker / "settings.json"};
    std::string error;
    expect(!store.save(Settings{LibraryMode::Xmb}, error) && !error.empty(),
           "a save that cannot happen says why");
    expect(read(blocker) == "a file where a directory should be", "and leaves what was there");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "settings";
    fs::remove_all(root);
    defaults(root);
    roundTrip(root);
    damaged(root);
    unwritable(root);
    fs::remove_all(root);
    std::printf("settings: all checks passed\n");
    return 0;
}

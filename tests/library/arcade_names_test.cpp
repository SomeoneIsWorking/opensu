// The arcade listing: parsing a libretro-database clrmamepro .dat and keeping it parsed.
#include "arcade_names.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;
using opensu::library::roms::NameDb;
using opensu::library::roms::NameMap;
using opensu::library::roms::parseDat;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

// The layout of libretro-database's metadat files, tabs and all.
constexpr const char* dat = "clrmamepro (\n"
                            "\tname \"MAME - Fixture\"\n"
                            "\tversion 2017-02-14\n"
                            ")\n"
                            "\n"
                            "game (\n"
                            "\tname \"Street Fighter II: The World Warrior (World 910522)\"\n"
                            "\tyear \"1991\"\n"
                            "\tdeveloper \"Capcom\"\n"
                            "\trom ( name sf2.zip size 3551819 crc B62D0BC7 md5 0 sha1 0 )\n"
                            ")\n"
                            "\n"
                            "game (\n"
                            "\tname \"Alien vs. Predator (USA 940520)\"\n"
                            "\trom ( name AVSPU.zip size 1 crc 0 )\n"
                            ")\n"
                            "\r\n"
                            "game (\r\n"
                            "\tname \"Asuka & Asuka (Japan)\"\r\n"
                            "\trom ( name asuka.zip size 1 crc 0 )\r\n"
                            ")\r\n"
                            "game (\n"
                            "\tname \"Not a zip\"\n"
                            "\trom ( name readme.txt size 1 crc 0 )\n"
                            ")\n"
                            "game (\n"
                            "\tname \"Street Fighter II: Later Listing\"\n"
                            "\trom ( name sf2.zip size 1 crc 0 )\n"
                            ")\n";

} // namespace

int main() {
    const NameMap names = parseDat(dat);
    expect(names.size() == 3, "a zip's game is listed once, anything else is skipped");
    expect(names.at("sf2") == "Street Fighter II: The World Warrior (World 910522)",
           "the first description of a short name stays");
    expect(names.at("avspu") == "Alien vs. Predator (USA 940520)",
           "short names are lower-case without .zip");
    expect(names.at("asuka") == "Asuka & Asuka (Japan)", "CRLF lines read the same");

    const fs::path dir = fs::temp_directory_path() / "opensu-arcade-names-test";
    fs::remove_all(dir);
    const NameDb db = NameDb::under(dir);
    expect(!db.cached() && db.load().empty(), "nothing is kept before the first save");
    std::string error;
    expect(db.save(names, error), "the names are kept");
    expect(db.cached() && db.load() == names, "what is kept loads back unchanged");
    const NameDb nowhere;
    expect(!nowhere.enabled() && !nowhere.cached() && nowhere.load().empty() &&
               !nowhere.save(names, error),
           "a database with no folder keeps nothing");
    fs::remove_all(dir);
    std::printf("arcade_names: all checks passed\n");
    return 0;
}

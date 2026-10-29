// Console art themes from folders: listing, opening, and that a theme's art is only its own.
#include "console_themes.hpp"
#include "support/test_main.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using opensu::artwork::CardStyle;
using opensu::artwork::ConsoleThemes;
using opensu::artwork::ThemeArt;
using opensu::artwork::ThemeInfo;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

std::vector<std::string> idsOf(const std::vector<ThemeInfo>& themes) {
    std::vector<std::string> ids;
    ids.reserve(themes.size());
    for (const ThemeInfo& theme : themes) {
        ids.push_back(theme.id);
    }
    return ids;
}

bool isKind(const std::optional<ThemeArt>& art, ThemeArt::Kind kind) {
    return art && art->kind == kind;
}

bool isFile(const std::optional<ThemeArt>& art, const fs::path& file) {
    return art && art->file == file;
}

} // namespace

int main() try {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "console-theme";
    fs::remove_all(root);
    const fs::path shipped = root / "shipped";
    const fs::path user = root / "user";
    write(shipped / "drawings" / "theme.json", R"({"name": "Drawings", "note": "ours"})");
    write(shipped / "drawings" / "cards" / "snes.svg", "<svg/>");
    write(shipped / "drawings" / "cards" / "ps4.svg", "<svg/>");
    write(shipped / "drawings" / "glyphs" / "snes.svg", "<svg/>");
    write(shipped / "drawings" / "glyphs" / "gc.svg", "<svg/>");
    write(shipped / "photos" / "theme.json", R"({"name": "Photos", "glyphs": "drawings"})");
    write(shipped / "photos" / "cards" / "snes.webp", "RIFF");
    write(shipped / "photos" / "cards" / "gc.png", "PNG");
    write(user / "mine" / "theme.json", R"({"name": "Mine", "cards": "tile"})");
    write(user / "mine" / "cards" / "nes.png", "PNG");
    write(user / "no-manifest" / "cards" / "nes.png", "PNG");
    write(user / "iiSU" / "notes.txt", "not a pack");
    const ConsoleThemes themes{shipped, user};

    expect(idsOf(themes.list()) == std::vector<std::string>{"photos", "drawings", "user/mine"},
           "the default leads, a folder needs its manifest, iiSU's needs its file placed");

    std::string error;
    const auto standard = themes.open("", error);
    expect(standard != nullptr && standard->info().id == "photos", "no id is the default theme");
    const auto drawings = themes.open("drawings", error);
    expect(drawings != nullptr && drawings->cardStyle() == CardStyle::Mark,
           "a shipped theme opens by its folder name");
    expect(isKind(drawings->card("snes"), ThemeArt::Kind::Mask), "an SVG card is a mask");

    const auto photos = themes.open("photos", error);
    expect(photos != nullptr && isKind(photos->card("snes"), ThemeArt::Kind::Picture) &&
               isFile(photos->card("gc"), shipped / "photos" / "cards" / "gc.png"),
           "a raster card is a picture from its file");
    expect(!photos->card("ps4"),
           "a card the theme lacks is none, though the default theme has one");
    expect(isFile(photos->glyph("gc"), shipped / "drawings" / "glyphs" / "gc.svg"),
           "a theme may name the shipped theme whose glyphs it uses");

    const auto mine = themes.open("user/mine", error);
    expect(mine != nullptr && mine->cardStyle() == CardStyle::Tile && mine->card("nes") &&
               !mine->card("snes") && !mine->glyph("snes"),
           "the player's theme has only its own cards and glyphs");

    expect(themes.open("../user/mine", error) == nullptr && !error.empty() &&
               themes.open("user/../photos", error) == nullptr,
           "an id cannot reach outside the theme folders");
    expect(themes.open("absent", error) == nullptr && !error.empty(), "a missing theme says why");
    expect(themes.open("iisu", error) == nullptr && error.find("iiSU") != std::string::npos,
           "iiSU's theme needs its file placed");
    fs::remove_all(root);
    std::printf("console_theme: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

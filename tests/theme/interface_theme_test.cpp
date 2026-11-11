// Interface themes: the shipped folders open in the format, a theme states only what it changes,
// and a manifest that breaks the format is refused with why.
#include "support/test_main.hpp"
#include "theme/interface_themes.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using opensu::theme::InterfaceTheme;
using opensu::theme::InterfaceThemes;
using opensu::theme::parseTheme;

void expect(bool condition, const std::string& what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        std::exit(1);
    }
}

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

/// The refusal `manifest` meets in `folder`, empty when it parses.
std::string refusal(const std::string& manifest, const fs::path& folder) {
    std::string error;
    const std::optional<InterfaceTheme> theme = parseTheme(manifest, folder, "t", error);
    return theme ? std::string{} : error;
}

void shippedThemesOpen() {
    const InterfaceThemes themes{fs::path{OPENSU_ASSETS_DIR} / "interface-themes",
                                 fs::path{OPENSU_TEST_SCRATCH} / "no-user-themes"};
    const std::vector<std::shared_ptr<const InterfaceTheme>> listed = themes.list();
    expect(!listed.empty() && listed.front()->id == "iisu" && listed.front()->name == "iiSU",
           "iiSU's look is shipped and listed first");
    std::string error;
    const std::shared_ptr<const InterfaceTheme> fallback = themes.open({}, error);
    expect(fallback != nullptr && fallback->id == "iisu", "no choice opens iiSU's look: " + error);
    InterfaceTheme defaults;
    defaults.id = fallback->id;
    defaults.name = fallback->name;
    defaults.author = fallback->author;
    defaults.note = fallback->note;
    defaults.folder = fallback->folder;
    expect(*fallback == defaults, "iiSU's look is the tokens' defaults");
    for (const std::shared_ptr<const InterfaceTheme>& theme : listed) {
        expect(themes.open(theme->id, error) != nullptr, theme->id + " opens: " + error);
    }
}

void aThemeStatesWhatItChanges(const fs::path& root) {
    const fs::path folder = root / "small";
    write(folder / "face.ttf", "not checked here");
    write(folder / "loop.ogg", "not checked here");
    fs::create_directories(folder / "sounds");
    std::string error;
    const std::optional<InterfaceTheme> theme =
        parseTheme(R"({"format": 1, "name": "Small", "palette": {"accent": "#3CB4E580"},
                       "type": {"face": "face.ttf", "body": 16}, "tiles": {"style": "channel"},
                       "home": {"scroll": "pages", "clockBar": true},
                       "sounds": {"set": "glass", "folder": "sounds"}, "music": "loop.ogg"})",
                   folder, "small", error);
    expect(theme.has_value(), "a small theme parses: " + error);
    if (!theme) {
        return;
    }
    const InterfaceTheme& small = *theme;
    const InterfaceTheme defaults;
    expect(small.palette.accent == opensu::theme::Colour{0x3C, 0xB4, 0xE5, 0x80} &&
               small.palette.ink == defaults.palette.ink,
           "a colour set is taken, the rest keep their defaults");
    expect(small.type.face == fs::weakly_canonical(folder / "face.ttf") &&
               small.type.body == 16.0f && small.type.title == defaults.type.title,
           "the face is the folder's file and one size changes");
    expect(small.tiles.style == opensu::theme::TileStyle::Channel && small.home.pages &&
               small.home.clockBar && small.home.pillClock,
           "the tiles and Home take the options set");
    expect(small.sounds.set == opensu::settings::SoundSet::Glass &&
               small.sounds.folder == fs::weakly_canonical(folder / "sounds") &&
               small.music == fs::weakly_canonical(folder / "loop.ogg"),
           "the sounds and the music are the folder's");
}

void brokenManifestsAreRefused(const fs::path& root) {
    const fs::path folder = root / "broken";
    write(folder / "face.ttf", "x");
    write(folder / "face.png", "x");
    write(root / "outside.ttf", "x");
    fs::create_directories(folder);
    std::error_code ec;
    fs::create_symlink(root / "outside.ttf", folder / "link.ttf", ec);
    const std::vector<std::pair<std::string, std::string>> cases{
        {"[]", "is not a JSON object"},
        {"{", "is not a JSON object"},
        {R"({"name": "X"})", "format is not 1"},
        {R"({"format": 2, "name": "X"})", "format is not 1"},
        {R"({"format": 1})", "name is missing"},
        {R"({"format": 1, "name": "X", "script": "run.sh"})", "script is not a key"},
        {R"({"format": 1, "name": "X", "palette": {"glow": "#FFFFFF"}})", "palette.glow is not"},
        {R"({"format": 1, "name": "X", "palette": {"ink": "red"}})", "palette.ink is not a colour"},
        {R"({"format": 1, "name": "X", "palette": {"ink": "#GG0000"}})", "is not a colour"},
        {R"({"format": 1, "name": "X", "palette": []})", "palette is not an object"},
        {R"({"format": 1, "name": "X", "type": {"body": 900}})", "type.body is outside"},
        {R"({"format": 1, "name": "X", "type": {"body": "big"}})", "type.body is not a number"},
        {R"({"format": 1, "name": "X", "tiles": {"grow": 3}})", "tiles.grow is outside"},
        {R"({"format": 1, "name": "X", "tiles": {"style": "neon"}})", "not glass or channel"},
        {R"({"format": 1, "name": "X", "home": {"scroll": "both"}})", "not pages or scrolling"},
        {R"({"format": 1, "name": "X", "home": {"clockBar": 1}})", "is not true or false"},
        {R"({"format": 1, "name": "X", "sounds": {"set": "iisu"}})", "not marimba, glass or"},
        {R"({"format": 1, "name": "X", "type": {"face": "../outside.ttf"}})", "not a path inside"},
        {R"({"format": 1, "name": "X", "type": {"face": "/etc/passwd"}})", "not a path inside"},
        {R"({"format": 1, "name": "X", "type": {"face": "a\\b.ttf"}})", "not a path inside"},
        {R"({"format": 1, "name": "X", "type": {"face": "face.png"}})", "is not a .ttf or .otf"},
        {R"({"format": 1, "name": "X", "type": {"face": "gone.ttf"}})", "names no file"},
        {R"({"format": 1, "name": "X", "sounds": {"folder": "none"}})", "names no folder"},
        {R"({"format": 1, "name": "X", "music": "face.ttf"})", "is not a .ogg or .wav"},
        {R"({"format": 1, "name": "X\u0001"})", "holds a control character"},
        {R"({"format": 1, "name": ")" + std::string(60, 'n') + R"("})", "longer than 48"},
        {std::string(opensu::theme::manifestLimit + 1, ' '), "larger than 64 KiB"},
    };
    for (const auto& [manifest, why] : cases) {
        const std::string error = refusal(manifest, folder);
        std::string what = manifest.substr(0, 80);
        what += " is refused for \"" + why + "\", not \"";
        what += error + "\"";
        expect(error.find(why) != std::string::npos, what);
    }
    if (!ec) {
        expect(refusal(R"({"format": 1, "name": "X", "type": {"face": "link.ttf"}})", folder)
                       .find("leads out of the theme's folder") != std::string::npos,
               "a link out of the folder is refused");
    }
}

void idsNameOnlyThemeFolders(const fs::path& root) {
    write(root / "shipped" / "iisu" / "theme.json", R"({"format": 1, "name": "iiSU"})");
    write(root / "shipped" / "calm" / "theme.json", R"({"format": 1, "name": "Calm"})");
    write(root / "user" / "own" / "theme.json", R"({"format": 1, "name": "Own"})");
    write(root / "user" / "bad" / "theme.json", R"({"format": 1, "name": "Bad", "x": 1})");
    const InterfaceThemes themes{root / "shipped", root / "user"};
    std::vector<std::string> ids;
    for (const std::shared_ptr<const InterfaceTheme>& theme : themes.list()) {
        ids.push_back(theme->id);
    }
    expect(ids == std::vector<std::string>{"iisu", "calm", "user/own"},
           "the default leads the shipped, then the player's, a broken one left out");
    std::string error;
    for (const char* id :
         {"user/../shipped/calm", "user/", "../user/own", "a/b", "..", "user/a/b"}) {
        error.clear();
        expect(themes.open(id, error) == nullptr && !error.empty(),
               std::string{id} + " names no theme");
    }
    const std::shared_ptr<const InterfaceTheme> broken = themes.openOrDefault("user/bad");
    expect(broken != nullptr && broken->id == "iisu", "a broken choice falls back to the default");
}

} // namespace

int main() try {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "interface-theme";
    fs::remove_all(root);
    shippedThemesOpen();
    aThemeStatesWhatItChanges(root / "small");
    brokenManifestsAreRefused(root / "broken");
    idsNameOnlyThemeFolders(root / "ids");
    fs::remove_all(root);
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

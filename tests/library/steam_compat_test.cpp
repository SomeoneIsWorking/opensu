// The tool Steam runs each installed game under, against a fake Steam root: the player's per-game
// pick, Valve's per-game mapping through a tool's alias, the default for Windows-only games only, a
// custom tool and an official Proton found where they are installed, a tool that is not installed,
// and both appinfo.vdf formats.
#include "library/steam_compat.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "library/steam_appinfo.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
namespace steam = opensu::library::steam;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void write(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream{path, std::ios::binary} << body;
}

/// Writes binary KeyValues the way Steam does, keys inline (28) or in a string table (29).
class AppInfoWriter {
  public:
    explicit AppInfoWriter(bool table) : table_{table} {
    }

    std::string text(const std::string& key, const std::string& value) {
        return std::string(1, '\x01') + keyOf(key) + value + std::string(1, '\0');
    }
    std::string number(const std::string& key, std::int32_t value) {
        return std::string(1, '\x02') + keyOf(key) + bytes(value);
    }
    std::string block(const std::string& key, const std::string& inner) {
        return std::string(1, '\0') + keyOf(key) + inner + std::string(1, '\x08');
    }
    void app(std::uint32_t id, const std::string& inner) {
        const std::string kv = block("appinfo", inner) + std::string(1, '\x08');
        apps_ += bytes(id) + bytes(static_cast<std::uint32_t>(60 + kv.size())) +
                 std::string(60, '\0') + kv;
    }
    [[nodiscard]] std::string file() const {
        std::string head =
            bytes(std::uint32_t{table_ ? 0x07564429U : 0x07564428U}) + bytes(std::uint32_t{1});
        const std::string body = apps_ + bytes(std::uint32_t{0});
        if (!table_) {
            return head + body;
        }
        std::string strings = bytes(static_cast<std::uint32_t>(keys_.size()));
        for (const std::string& key : keys_) {
            strings += key + std::string(1, '\0');
        }
        const std::size_t stringsAt = head.size() + 8 + body.size();
        return head + bytes(static_cast<std::int64_t>(stringsAt)) + body + strings;
    }

  private:
    template <typename T> static std::string bytes(T value) {
        std::string out(sizeof(T), '\0');
        std::memcpy(out.data(), &value, sizeof(T));
        return out;
    }
    std::string keyOf(const std::string& key) {
        if (!table_) {
            return key + std::string(1, '\0');
        }
        std::size_t at = 0;
        while (at < keys_.size() && keys_[at] != key) {
            ++at;
        }
        if (at == keys_.size()) {
            keys_.push_back(key);
        }
        return bytes(static_cast<std::int32_t>(at));
    }

    bool table_;
    std::vector<std::string> keys_;
    std::string apps_;
};

fs::path fakeSteam(const fs::path& base, bool table) {
    const fs::path root = base / (table ? "steam29" : "steam28");
    write(root / "config" / "config.vdf",
          R"("InstallConfigStore" { "Software" { "Valve" { "Steam" { "CompatToolMapping" {
              "0" { "name" "proton_10" "config" "" "priority" "75" }
              "300" { "name" "GE-Proton11-5-x86_64" "config" "" "priority" "250" }
              "400" { "name" "GE-Proton10-29" "config" "" "priority" "250" }
          } } } } })");
    write(root / "compatibilitytools.d" / "GE-Proton11-5-x86_64" / "compatibilitytool.vdf",
          R"("compatibilitytools" { "compat_tools" { "GE-Proton11-5-x86_64" {
              "install_path" "." "display_name" "GE-Proton11-5" } } })");
    write(root / "steamapps" / "appmanifest_3658110.acf",
          R"("AppState" { "appid" "3658110" "name" "Proton 10.0" "StateFlags" "4"
              "installdir" "Proton 10.0" })");
    fs::create_directories(root / "steamapps" / "common" / "Proton 10.0");

    AppInfoWriter info{table};
    const auto game = [&info](std::uint32_t id, const std::string& platforms) {
        info.app(id, info.block("common", info.text("oslist", platforms)));
    };
    game(100, "windows");
    game(200, "windows,linux");
    game(300, "linux");
    game(400, "windows");
    game(500, "windows,macos");
    game(700, "windows,macos,linux");
    game(800, "windows");
    const auto mapped = [&info](const std::string& app, const std::string& tool) {
        return info.block(app, info.text("appid", app) + info.text("platform", "linux") +
                                   info.text("tool", tool) + info.text("config", "none"));
    };
    info.app(891390,
             info.block("extended",
                        info.block("compat_tools",
                                   info.block("proton_10",
                                              info.number("appid", 3658110) +
                                                  info.text("aliases", "proton-10,proton-stable")) +
                                       info.block("proton_9", info.number("appid", 2805730))) +
                            info.block("app_mappings", mapped("300", "proton-stable") +
                                                           mapped("700", "proton-stable") +
                                                           mapped("800", ""))));
    write(root / "appcache" / "appinfo.vdf", info.file());
    return root;
}

void picksPerGame(const fs::path& base, bool table) {
    const fs::path root = fakeSteam(base, table);
    const steam::Library library = steam::Library::discover(base, {root});
    const std::map<std::string, steam::CompatTool, std::less<>> tools =
        steam::compatTools(library, {"100", "200", "300", "400", "500", "600", "700", "800"});
    const auto at = [&tools](const char* app) {
        const auto found = tools.find(app);
        return found != tools.end() ? found->second : steam::CompatTool{.name = "-", .dir = {}};
    };
    expect(at("100").name == "proton_10" && at("100").dir == root / "steamapps/common/Proton 10.0",
           "a Windows-only game runs under the default, found through its app manifest");
    expect(at("500").name == "proton_10", "a Windows and macOS game is Windows-only here");
    expect(!tools.contains("200"), "a game with a Linux build runs as it is");
    expect(at("300").name == "GE-Proton11-5-x86_64" &&
               at("300").dir == root / "compatibilitytools.d" / "GE-Proton11-5-x86_64",
           "the player's pick wins, even over a Linux build, and a custom tool is found");
    expect(at("400").name == "GE-Proton10-29" && at("400").dir.empty(),
           "a picked tool that is not installed has no folder");
    expect(!tools.contains("600"), "a game appinfo does not know is left alone");
    expect(at("700").name == "proton_10" && at("700").dir == root / "steamapps/common/Proton 10.0",
           "Valve's mapping runs a game with a Linux build under the tool its alias names");
    expect(at("800").name == "proton_10", "a mapping Valve emptied leaves the default");
}

void readsOnlyWhatIsAsked(const fs::path& base) {
    const fs::path root = fakeSteam(base, true);
    const auto apps = steam::readAppInfo(root / "appcache" / "appinfo.vdf", {"200", "891390"});
    expect(apps.size() == 2 && apps.at("200").str({"common", "oslist"}) == "windows,linux" &&
               apps.at("891390").str({"extended", "compat_tools", "proton_9", "appid"}) ==
                   "2805730",
           "the apps asked for, numbers as text");
    write(base / "broken.vdf", std::string("\x29\x44\x56\x07\x01\0\0\0\xff\xff", 10));
    expect(steam::readAppInfo(base / "broken.vdf", {"200"}).empty(), "a broken file reads empty");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "steam-compat"};
    picksPerGame(scratch.path(), true);
    picksPerGame(scratch.path(), false);
    readsOnlyWhatIsAsked(scratch.path());
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

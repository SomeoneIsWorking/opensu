// The GOG install records, the folder an uninstall may delete, and the token as gogdl's
// `--auth-config-path` file reads it.
#include "library/gog_installs.hpp"
#include "library/gogdl_auth.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include <sys/stat.h>

namespace {

namespace fs = std::filesystem;
using opensu::library::gog::GogdlAuthFile;
using opensu::library::gog::Installed;
using opensu::library::gog::InstallRecords;
using opensu::library::gog::ownedFolder;
using opensu::library::gog::Paths;
using opensu::library::gog::Token;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void testRecords(const fs::path& dir) {
    const InstallRecords records{Paths::under(dir).records};
    expect(records.all().empty() && !records.find("1"), "no file is no installs");
    records.add("1", Installed{.path = "/games/One", .platform = "linux"});
    records.add("2", Installed{.path = "/games/Two", .platform = "windows"});
    records.add("1", Installed{.path = "/games/One2", .platform = "windows"});
    const InstallRecords again{Paths::under(dir).records};
    expect(again.all().size() == 2, "records accumulate");
    const auto one = again.find("1");
    expect(one && one->path == "/games/One2" && one->platform == "windows",
           "a record is replaced by the later install of its game");
    const auto two = again.find("2");
    expect(two && two->platform == "windows", "the others are kept");
}

void testRemove(const fs::path& dir) {
    const InstallRecords records{Paths::under(dir).records};
    records.remove("1");
    expect(records.all().empty(), "forgetting a game with no record changes nothing");
    records.add("1", Installed{.path = "/games/One", .platform = "linux"});
    records.add("2", Installed{.path = "/games/Two", .platform = "windows"});
    records.remove("1");
    const InstallRecords again{Paths::under(dir).records};
    expect(!again.find("1") && again.find("2"), "a removed record is gone and the rest kept");
}

void testGameRoot() {
    const Paths paths = Paths::under("/data");
    expect(paths.gameRoot(std::nullopt, "7") == fs::path{"/data/gog-games/7"},
           "with no folder a game's root is under the data directory");
    expect(paths.gameRoot(fs::path{"/mnt/games"}, "7") == fs::path{"/mnt/games/7"},
           "with a folder it is under that folder");
}

void testOwnedFolder(const fs::path& dir) {
    const fs::path root = dir / "games" / "7";
    fs::create_directories(root / "Game");
    const auto inside = ownedFolder(Installed{.path = root / "Game", .platform = "linux"}, root);
    expect(inside && *inside == fs::canonical(root / "Game"),
           "a folder inside the game's root may be deleted");
    expect(!ownedFolder(Installed{.path = root, .platform = "linux"}, root),
           "the root itself is not the game's folder");
    fs::create_directories(dir / "elsewhere" / "Game");
    expect(!ownedFolder(Installed{.path = dir / "elsewhere" / "Game", .platform = "linux"}, root),
           "a folder outside the root is only forgotten");
    fs::create_directory_symlink(dir / "elsewhere" / "Game", root / "Link");
    expect(!ownedFolder(Installed{.path = root / "Link", .platform = "linux"}, root),
           "a symlink in the root to a folder outside it is only forgotten");
    fs::create_directories(dir / "games");
    fs::create_directory_symlink(dir / "elsewhere", dir / "games" / "8");
    expect(!ownedFolder(Installed{.path = dir / "games" / "8" / "Game", .platform = "linux"},
                        dir / "games" / "8"),
           "a game root that is itself a symlink out of the folder leads nowhere");
    fs::create_directories(dir / "real-folder" / "9" / "Game");
    fs::create_directory_symlink(dir / "real-folder", dir / "linked-folder");
    expect(ownedFolder(Installed{.path = dir / "linked-folder" / "9" / "Game", .platform = "linux"},
                       dir / "linked-folder" / "9")
               .has_value(),
           "an install folder reached through a symlink still holds its games");
    expect(!ownedFolder(Installed{.path = root / "Missing", .platform = "linux"}, root),
           "a folder that is gone has nothing to delete");
    expect(!ownedFolder(
               Installed{.path = root / ".." / ".." / "elsewhere" / "Game", .platform = "linux"},
               root),
           "a path that climbs out of the root is only forgotten");
}

void testAuthFile(const fs::path& dir) {
    const GogdlAuthFile file{Paths::under(dir).gogdlAuth};
    expect(!file.read(), "no file holds no token");
    file.write(Token{"ACCESS", "REFRESH", "4242", 5000}, 2000);

    struct stat info{};
    expect(::stat(file.file().c_str(), &info) == 0 && (info.st_mode & 0777) == 0600,
           "the file is owner-only");
    std::ifstream in{file.file()};
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    expect(text.starts_with(R"({"46899977096215655":{)") &&
               text.find(R"("access_token":"ACCESS")") != std::string::npos &&
               text.find(R"("refresh_token":"REFRESH")") != std::string::npos &&
               text.find(R"("user_id":"4242")") != std::string::npos,
           "gogdl finds the tokens under GOG Galaxy's client id");
    expect(text.find(R"("loginTime":2000)") != std::string::npos &&
               text.find(R"("expires_in":3000)") != std::string::npos,
           "gogdl's expiry (loginTime + expires_in) is the token's");

    const std::optional<Token> back = file.read();
    expect(back && back->accessToken == "ACCESS" && back->refreshToken == "REFRESH" &&
               back->userId == "4242" && back->expiresAt == 5000,
           "what gogdl holds reads back as the token");

    file.remove();
    expect(!fs::exists(file.file()), "the file can be deleted");
}

} // namespace

int main() {
    const fs::path dir = fs::current_path() / "gog-install-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    testRecords(dir / "records");
    testRemove(dir / "remove");
    testGameRoot();
    testOwnedFolder(dir / "owned");
    testAuthFile(dir / "auth");
    fs::remove_all(dir, ec);
    std::printf("gog_install: all checks passed\n");
    return 0;
}

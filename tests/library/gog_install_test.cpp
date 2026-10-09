// The GOG install records and the token as gogdl's `--auth-config-path` file reads it.
#include "library/gog_installs.hpp"
#include "library/gogdl_auth.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <sys/stat.h>

namespace {

namespace fs = std::filesystem;
using opensu::library::gog::GogdlAuthFile;
using opensu::library::gog::Installed;
using opensu::library::gog::InstallRecords;
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
    testAuthFile(dir / "auth");
    fs::remove_all(dir, ec);
    std::printf("gog_install: all checks passed\n");
    return 0;
}

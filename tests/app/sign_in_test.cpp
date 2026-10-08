// The sign-in owner, with a fake browser opener, a fake Legendary and a local server for GOG.
#include "sign_in.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "library/gog_token.hpp"
#include "support/loopback_server.hpp"

namespace {

namespace fs = std::filesystem;
using iideck::app::SignInResult;
using iideck::app::Store;
using iideck::app::StoreSignIn;
using iideck::test::LoopbackServer;
using iideck::test::reply;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

/// A program that writes its arguments, one per line, to `record` and exits with `status`.
fs::path fakeProgram(const fs::path& dir, const std::string& name, int status) {
    const fs::path script = dir / name;
    std::ofstream out{script};
    out << "#!/bin/sh\nfor a in \"$@\"; do echo \"$a\"; done > '"
        << (dir / (name + ".args")).string() << "'\nexit " << status << "\n";
    out.close();
    fs::permissions(script, fs::perms::owner_all);
    return script;
}

fs::path freshDir() {
    const fs::path dir = fs::current_path() / "sign-in-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

} // namespace

int main() {
    const fs::path dir = freshDir();
    LoopbackServer gog{[](const lucent::http::Request& request) {
        const std::string& target = request.target;
        if (target.find("code=GOOD-CODE&") != std::string::npos) {
            return reply(
                200, R"({"access_token":"A","refresh_token":"R","expires_in":3600,"user_id":"7"})");
        }
        return reply(400, "{}");
    }};
    const fs::path browser = fakeProgram(dir, "browser", 0);
    const fs::path legendary = fakeProgram(dir, "legendary", 0);
    StoreSignIn::Options options;
    options.dataDir = dir / "data";
    options.gog = {gog.base(), gog.base()};
    options.browser = browser.string();
    options.legendary = legendary.string();
    StoreSignIn signIn{options};

    const SignInResult gogPage = signIn.open(Store::Gog);
    expect(gogPage.ok && read(dir / "browser.args") ==
                             gog.base() + "/auth?client_id=46899977096215655&redirect_uri=https%3A"
                                          "%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"
                                          "&response_type=code&layout=client2\n",
           "GOG's page opens in the browser opener");
    const SignInResult epicPage = signIn.open(Store::Epic);
    expect(epicPage.ok && read(dir / "browser.args") == "https://legendary.gl/epiclogin\n",
           "Epic's page opens in the browser opener");

    const SignInResult gogDone = signIn.complete(Store::Gog, "GOOD-CODE");
    expect(gogDone.ok, "GOG's code is exchanged");
    expect(contains(read(dir / "data" / "gog-token.json"), "\"access_token\":\"A\""),
           "GOG's token is kept in the data directory");
    const SignInResult gogRefused = signIn.complete(Store::Gog, "BAD-CODE");
    expect(!gogRefused.ok && !contains(gogRefused.message, "BAD-CODE"),
           "a refused GOG code fails without repeating it");

    const SignInResult epicDone = signIn.complete(Store::Epic, "EPIC-CODE");
    expect(epicDone.ok && read(dir / "legendary.args") == "auth\n--code\nEPIC-CODE\n",
           "Epic's code goes to legendary auth");
    expect(!contains(epicDone.message, "EPIC-CODE"), "the result never repeats the code");

    options.legendary = fakeProgram(dir, "legendary-fails", 1).string();
    options.browser = (dir / "no-such-browser").string();
    StoreSignIn failing{options};
    expect(!failing.complete(Store::Epic, "EPIC-CODE").ok, "a failing legendary is reported");
    expect(!failing.open(Store::Gog).ok, "a missing browser opener is reported");

    std::printf("sign-in: all checks passed\n");
    return 0;
}

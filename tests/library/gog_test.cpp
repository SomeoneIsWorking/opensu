// The GOG sign-in and library against a local server standing in for auth.gog.com and
// embed.gog.com.
#include "library/gog.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "support/loopback_server.hpp"

namespace {

namespace fs = std::filesystem;
using iideck::library::Game;
using iideck::library::gog::Auth;
using iideck::library::gog::Endpoints;
using iideck::library::gog::NotSignedIn;
using iideck::library::gog::Provider;
using iideck::library::gog::Token;
using iideck::library::gog::TokenStore;
using iideck::net::WebClient;
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

constexpr const char* fakeToken =
    R"({"access_token":"ACCESS-1","refresh_token":"REFRESH-1","expires_in":3600,"user_id":"4242"})";
constexpr const char* refreshedToken =
    R"({"access_token":"ACCESS-2","refresh_token":"REFRESH-2","expires_in":3600,"user_id":"4242"})";

/// Stands in for GOG: a good code and a good refresh token buy a token, `ACCESS-2` or `ACCESS-1`
/// opens the library, which has two pages.
class FakeGog {
  public:
    FakeGog()
        : server_{[this](const lucent::http::Request& request) {
              return answer(request);
          }} {
    }

    [[nodiscard]] Endpoints endpoints() const {
        return {server_.base(), server_.base()};
    }
    [[nodiscard]] int tokenRequests() const {
        return tokenRequests_;
    }
    [[nodiscard]] int libraryRequests() const {
        return libraryRequests_;
    }
    [[nodiscard]] std::string lastTokenTarget() const {
        const std::lock_guard lock{mutex_};
        return lastTokenTarget_;
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::string& target = request.target;
        if (target.rfind("/token?", 0) == 0) {
            ++tokenRequests_;
            {
                const std::lock_guard lock{mutex_};
                lastTokenTarget_ = target;
            }
            if (contains(target, "grant_type=authorization_code&code=GOOD-CODE&")) {
                return reply(200, fakeToken);
            }
            if (contains(target, "grant_type=refresh_token&refresh_token=REFRESH-1")) {
                return reply(200, refreshedToken);
            }
            return reply(400, R"({"error":"invalid_grant"})");
        }
        if (target.rfind("/account/getFilteredProducts?mediaType=1&page=", 0) == 0) {
            ++libraryRequests_;
            if (!request.header("Authorization").value_or("").starts_with("Bearer ACCESS-")) {
                return reply(401, "");
            }
            if (contains(target, "page=1")) {
                return reply(200, R"({"totalPages":2,"products":[
                    {"id":1207658930,"title":"Alpha","image":"//images-4.gog.com/aaa"},
                    {"id":1,"title":"","image":"//images-4.gog.com/skipped"}]})");
            }
            return reply(200, R"({"totalPages":2,"products":[
                {"id":2,"title":"Beta","image":"//images-2.gog.com/bbb"}]})");
        }
        return reply(404, "");
    }

    mutable std::mutex mutex_;
    std::string lastTokenTarget_;
    std::atomic<int> tokenRequests_{0};
    std::atomic<int> libraryRequests_{0};
    LoopbackServer server_;
};

fs::path freshDir(const char* name) {
    const fs::path dir = fs::current_path() / "gog-test" / name;
    fs::remove_all(dir);
    return dir;
}

void testSignIn() {
    FakeGog gog;
    const fs::path dir = freshDir("sign-in");
    const WebClient web;
    const TokenStore store = TokenStore::under(dir);
    const Auth auth{store, gog.endpoints(), web};

    expect(auth.loginUrl() ==
               gog.endpoints().auth +
                   "/auth?client_id=46899977096215655&redirect_uri=https%3A%2F%2Fembed.gog.com%2F"
                   "on_login_success%3Forigin%3Dclient&response_type=code&layout=client2",
           "the sign-in page is GOG Galaxy's client page");

    bool refused = false;
    try {
        auth.signIn("WRONG");
    } catch (const NotSignedIn&) {
        refused = true;
    }
    expect(refused && !store.load(), "a refused code saves nothing");

    auth.signIn("GOOD-CODE");
    expect(
        contains(gog.lastTokenTarget(),
                 "client_id=46899977096215655&client_secret=9d85c43b1482497dbbce61f6e4aa173a43379"
                 "6eeae2ca8c5f6129f2dc4de46d9&grant_type=authorization_code&code=GOOD-CODE&"
                 "redirect_uri=https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"),
        "the code is exchanged with GOG Galaxy's client and redirect");
    const std::optional<Token> saved = store.load();
    expect(saved && saved->accessToken == "ACCESS-1" && saved->refreshToken == "REFRESH-1" &&
               saved->userId == "4242" && saved->expiresAt > 1'700'000'000,
           "the token GOG granted is saved with its expiry");

    struct stat info{};
    expect(::stat(store.file().c_str(), &info) == 0 && (info.st_mode & 0777) == 0600,
           "the token file is owner-only");
    expect((fs::status(dir).permissions() & fs::perms::group_all & fs::perms::others_all) ==
               fs::perms::none,
           "the token's directory is owner-only");
    std::size_t files = 0;
    for ([[maybe_unused]] const auto& entry : fs::directory_iterator{dir}) {
        ++files;
    }
    expect(files == 1, "no staged file is left beside the token");
    expect(!contains(read(store.file()), "GOOD-CODE"), "the code is not stored");
}

void testRefresh() {
    FakeGog gog;
    const fs::path dir = freshDir("refresh");
    const WebClient web;
    const TokenStore store = TokenStore::under(dir);
    const Auth auth{store, gog.endpoints(), web};

    bool signedOut = false;
    try {
        (void)auth.accessToken();
    } catch (const NotSignedIn&) {
        signedOut = true;
    }
    expect(signedOut && gog.tokenRequests() == 0, "without a token there is nothing to ask");

    const std::int64_t later = 4'000'000'000;
    store.save(Token{"ACCESS-1", "REFRESH-1", "4242", later});
    expect(auth.accessToken() == "ACCESS-1" && gog.tokenRequests() == 0,
           "a token that has not expired is used as is");

    store.save(Token{"ACCESS-1", "REFRESH-1", "4242", 1});
    expect(auth.accessToken() == "ACCESS-2", "an expired token is refreshed");
    expect(gog.tokenRequests() == 1, "one request refreshes it");
    const std::optional<Token> saved = store.load();
    expect(saved && saved->accessToken == "ACCESS-2" && saved->refreshToken == "REFRESH-2" &&
               saved->expiresAt > 1'700'000'000,
           "the refreshed token replaces the saved one");
    expect(auth.accessToken() == "ACCESS-2" && gog.tokenRequests() == 1,
           "the refreshed token is not refreshed again");

    store.save(Token{"ACCESS-0", "REVOKED", "4242", 1});
    bool revoked = false;
    try {
        (void)auth.accessToken();
    } catch (const NotSignedIn&) {
        revoked = true;
    }
    expect(revoked && store.load()->refreshToken == "REVOKED",
           "a refresh GOG refuses is an error and keeps the saved token");
}

void testLibrary() {
    FakeGog gog;
    const fs::path dir = freshDir("library");
    Provider provider{TokenStore::under(dir), gog.endpoints()};

    bool signedOut = false;
    try {
        (void)provider.list();
    } catch (const NotSignedIn&) {
        signedOut = true;
    }
    expect(signedOut && gog.libraryRequests() == 0, "without a sign-in nothing is requested");

    TokenStore::under(dir).save(Token{"ACCESS-1", "REFRESH-1", "4242", 4'000'000'000});
    const std::vector<Game> games = provider.list();
    expect(gog.libraryRequests() == 2, "a request reads each page");
    expect(games.size() == 2, "a product without a title is skipped");
    expect(games[0].id == "gog:1207658930" && games[0].sourceId == "1207658930" &&
               games[0].title == "Alpha",
           "a game is keyed by its product id");
    expect(games[0].source == iideck::library::Source::Gog && !games[0].installed &&
               games[0].launch.empty(),
           "an owned game is listed as not installed");
    expect(games[0].artworkUrl == "https://images-4.gog.com/aaa" &&
               games[1].artworkUrl == "https://images-2.gog.com/bbb",
           "GOG's image stem is kept for later");
    expect(games[1].title == "Beta", "the second page follows the first");

    TokenStore::under(dir).save(Token{"WRONG", "REFRESH-1", "4242", 4'000'000'000});
    bool failed = false;
    try {
        (void)provider.list();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "a library GOG will not give is an error");
}

} // namespace

int main() {
    testSignIn();
    testRefresh();
    testLibrary();
    std::printf("gog: all checks passed\n");
    return 0;
}

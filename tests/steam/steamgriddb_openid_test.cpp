// What SteamGridDB's sign-in through Steam reads and sends over HTTP: Steam's OpenID form, the
// cookies each answer sets, the form as multipart, and the key in SteamGridDB's preferences.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "steamgriddb_openid.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::steam::Pairs;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void formFields() {
    const std::string page =
        R"(<form action="/search" id="search"><input name="q" value="x"></form>)"
        R"(<form enctype="multipart/form-data" action="https://steamcommunity.com/openid/login")"
        R"( method="POST" name="loginForm" id="openidForm">)"
        R"(<input type="hidden" name="action" value="steam_openid_login">)"
        R"(<input type="hidden" name="openid.mode" value="checkid_setup">)"
        R"(<input type="hidden" name="openidparams" value="a&amp;b=">)"
        R"(<input type="hidden" name="nonce" value="n1"><input class="btn" type="image" src="x">)"
        R"(</form><input name="after" value="no">)";
    const Pairs fields = opensu::steam::openIdFormFields(page);
    expect(fields == Pairs{{"action", "steam_openid_login"},
                           {"openid.mode", "checkid_setup"},
                           {"openidparams", "a&b="},
                           {"nonce", "n1"}},
           "the sign-in form's named fields, in order, entities undone, nothing outside it");
    expect(opensu::steam::openIdFormFields("<html>Sign in</html>").empty(),
           "a page without the form has no fields");
}

void cookies() {
    const Pairs headers{{"content-type", "text/html"},
                        {"Set-Cookie", "PHPSESSID=abc; path=/; HttpOnly"},
                        {"set-cookie", "XSRF-TOKEN=t%3D1; path=/"},
                        {"set-cookie", "broken"}};
    expect(opensu::steam::setCookies(headers) ==
               Pairs{{"PHPSESSID", "abc"}, {"XSRF-TOKEN", "t%3D1"}},
           "each Set-Cookie's name and value, whatever the header's case");
}

void multipart() {
    expect(opensu::steam::multipartBody({{"a", "1"}, {"b", "x y"}}, "B") ==
               "--B\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\n1\r\n"
               "--B\r\nContent-Disposition: form-data; name=\"b\"\r\n\r\nx y\r\n--B--\r\n",
           "each field a part, the last boundary closing it");
}

void prefsKey() {
    expect(
        opensu::steam::prefsApiKey(
            R"({"success":true,"data":{"user":{"api_key":"0123456789abcdef0123456789abcdef"}}})") ==
            "0123456789abcdef0123456789abcdef",
        "a signed-in account's key");
    expect(opensu::steam::prefsApiKey(R"({"data":{"user":{"api_key":null}}})") == "",
           "an account with no key yet");
    expect(!opensu::steam::prefsApiKey(R"({"data":{"user":null}})") &&
               !opensu::steam::prefsApiKey("<html>"),
           "no account: not signed in");
}

} // namespace

int main() try {
    formFields();
    cookies();
    multipart();
    prefsKey();
    std::puts("steamgriddb_openid: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

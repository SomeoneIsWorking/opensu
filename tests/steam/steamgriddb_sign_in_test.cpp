// The SteamGridDB sign-in through Steam's browser, against a fake Steam whose browser view is a
// DevTools page the fake moves from SteamGridDB's sign-in to Steam's OpenID page and back, as the
// real view does when the player confirms. The page scripts' answers are the fake's; what is
// checked is the flow: what is opened, where scripts run, what is pressed, what is read and closed.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "steamgriddb_sign_in.hpp"
#include "support/fake_steam.hpp"
#include "support/test_main.hpp"

namespace {

using nlohmann::json;
using opensu::steam::SteamGridDbSignIn;
using opensu::test::FakeSteam;
using Step = SteamGridDbSignIn::Step;
using namespace std::chrono_literals;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

constexpr const char* fakeKey = "0123456789abcdef0123456789abcdef";
constexpr const char* login = "https://www.steamgriddb.com/login";
constexpr const char* openId = "https://steamcommunity.com/openid/login?openid.mode=checkid_setup";
constexpr const char* home = "https://www.steamgriddb.com/";
constexpr const char* keyPage = "https://www.steamgriddb.com/profile/preferences/api";

constexpr SteamGridDbSignIn::Timing quick{10ms, 10s};

/// A desktop Steam whose `steam://openurl` opens view VIEW on SteamGridDB's sign-in.
struct Rig {
    FakeSteam* self{nullptr};
    FakeSteam steam{[this](const std::string& expression) {
        if (contains(expression, "ExecuteSteamURL")) {
            self->putPage("VIEW", login);
        }
        return json(true);
    }};
    Rig() {
        self = &steam;
    }
};

/// The view as SteamGridDB's pages answer the step script: "Login via Steam" goes to Steam's
/// OpenID page, any other page goes to the API page, which makes its key on the first press.
void answerAsSteamGridDb(FakeSteam& steam) {
    steam.answerPages([&steam, generated = std::make_shared<bool>(false)](
                          const std::string& page, const std::string& expression) -> json {
        const std::string at = steam.pageUrl(page);
        if (at == login) {
            if (contains(expression, "true ? pressable(/log")) {
                steam.putPage(page, openId);
                return {{"state", "login"}, {"clicked", true}};
            }
            return {{"state", "login"}};
        }
        if (at == keyPage) {
            if (*generated) {
                return {{"state", "key"}, {"key", fakeKey}};
            }
            *generated = contains(expression, "true ? pressable(/generate/i)");
            return {{"state", "nokey"}, {"clicked", *generated}};
        }
        steam.putPage(page, keyPage);
        return {{"state", "navigating"}};
    });
}

/// The player confirming Steam's page once the view shows it.
std::jthread confirmWhenAsked(FakeSteam& steam) {
    return std::jthread{[&steam](const std::stop_token& stop) {
        while (!stop.stop_requested() && steam.pageUrl("VIEW") != openId) {
            std::this_thread::sleep_for(1ms);
        }
        const int seen = steam.listings();
        while (!stop.stop_requested() && steam.listings() < seen + 3) {
            std::this_thread::sleep_for(1ms);
        }
        steam.putPage("VIEW", home);
    }};
}

void testSignsInAndReadsTheKey() {
    Rig rig;
    answerAsSteamGridDb(rig.steam);
    const std::jthread player = confirmWhenAsked(rig.steam);
    SteamGridDbSignIn signIn{rig.steam.port(), quick};
    std::string error;
    expect(signIn.reachable(error), "a running Steam answers");
    std::vector<Step> steps;
    const std::optional<std::string> key = signIn.run(
        std::stop_token{},
        [&steps](Step step) {
            steps.push_back(step);
        },
        error);
    expect(key && *key == fakeKey, "the key on the API page is read");
    expect(error.empty(), "a finished sign-in reports no error");
    expect(steps == std::vector<Step>{Step::WaitingForPlayer, Step::ReadingKey},
           "the player hears the wait, then the reading");

    const std::vector<std::string> calls = rig.steam.calls();
    expect(calls.size() == 2 && contains(calls[1], "SteamClient.URL.ExecuteSteamURL(") &&
               contains(calls[1], "\"steam://openurl/https://www.steamgriddb.com/login\""),
           "Steam's browser opens SteamGridDB's sign-in");
    const std::vector<FakeSteam::PageCall> pages = rig.steam.pageCalls();
    expect(!pages.empty(), "the view is asked");
    bool loginPressedOnce = true;
    int loginOffers = 0;
    for (const FakeSteam::PageCall& call : pages) {
        expect(call.page == "VIEW", "scripts run only in the view the sign-in opened");
        expect(call.url.starts_with("https://www.steamgriddb.com/"),
               "scripts run only while the view shows SteamGridDB, never Steam's page");
        expect(!contains(call.expression, "cookie") && !contains(call.expression, "localStorage"),
               "no script reads cookies or storage");
        expect(contains(call.expression, "location.origin !== origin"),
               "the script itself refuses another origin");
        loginOffers += contains(call.expression, "true ? pressable(/log") ? 1 : 0;
    }
    loginPressedOnce = loginOffers == 1;
    expect(loginPressedOnce, "Login via Steam is pressed once");
    expect(rig.steam.closed() == std::vector<std::string>{"VIEW"}, "the view is closed after");
    expect(rig.steam.pageUrl("VIEW").empty(), "the view is gone");
}

/// openSU's own Steam: on its desktop UI until Big Picture is asked for, and only Big Picture's
/// browser opens SteamGridDB.
struct BigPictureRig {
    FakeSteam* self{nullptr};
    std::atomic<bool> bigPicture{false};
    FakeSteam steam{[this](const std::string& expression) -> json {
        if (contains(expression, "GetUIMode")) {
            return json(bigPicture.load());
        }
        if (contains(expression, "steam://open/bigpicture")) {
            bigPicture = true;
        } else if (contains(expression, "ExitBigPictureMode")) {
            bigPicture = false;
        } else if (contains(expression, "steam://openurl/") && bigPicture) {
            self->putPage("VIEW", login);
        }
        return json(true);
    }};
    BigPictureRig() {
        self = &steam;
    }
};

std::vector<std::string> steamCalls(const FakeSteam& steam) {
    std::vector<std::string> named;
    for (const std::string& call : steam.calls()) {
        if (contains(call, "open/bigpicture")) {
            named.emplace_back("open big picture");
        } else if (contains(call, "openurl")) {
            named.emplace_back("open sign-in");
        } else if (contains(call, "ExitBigPictureMode")) {
            named.emplace_back("close big picture");
        }
    }
    return named;
}

void testOwnSteamSignsInThroughBigPicture() {
    BigPictureRig rig;
    answerAsSteamGridDb(rig.steam);
    const std::jthread player = confirmWhenAsked(rig.steam);
    SteamGridDbSignIn signIn{rig.steam.port(), quick, SteamGridDbSignIn::View::BigPicture};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        std::stop_token{},
        [](Step) {
        },
        error);
    expect(key && *key == fakeKey, "the key is read through Big Picture's browser");
    expect(steamCalls(rig.steam) ==
               std::vector<std::string>{"open big picture", "open sign-in", "close big picture"},
           "Big Picture opens before the sign-in and closes after it");
    expect(rig.steam.closed() == std::vector<std::string>{"VIEW"}, "the view is closed after");
}

void testBigPictureAlreadyUpStaysUp() {
    BigPictureRig rig;
    rig.bigPicture = true;
    rig.steam.answerPages([&rig](const std::string& page, const std::string&) -> json {
        rig.steam.putPage(page, openId);
        return {{"state", "login"}, {"clicked", true}};
    });
    std::stop_source cancel;
    SteamGridDbSignIn signIn{rig.steam.port(), quick, SteamGridDbSignIn::View::BigPicture};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        cancel.get_token(),
        [&cancel](Step step) {
            if (step == Step::WaitingForPlayer) {
                cancel.request_stop();
            }
        },
        error);
    expect(!key && error == "cancelled", "a cancel ends the sign-in");
    expect(steamCalls(rig.steam) == std::vector<std::string>{"open sign-in"},
           "a Big Picture the player had up is neither opened nor closed");
}

void testCancelWhileBigPictureOpensClosesIt() {
    std::stop_source cancel;
    // Big Picture never comes up before the cancel.
    FakeSteam stuck{[&cancel](const std::string& expression) -> json {
        if (contains(expression, "GetUIMode")) {
            cancel.request_stop();
            return false;
        }
        return json(true);
    }};
    SteamGridDbSignIn signIn{stuck.port(), quick, SteamGridDbSignIn::View::BigPicture};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        cancel.get_token(),
        [](Step) {
        },
        error);
    expect(!key && error == "cancelled", "a cancel while Big Picture opens ends the sign-in");
    expect(steamCalls(stuck) == std::vector<std::string>{"open big picture", "close big picture"},
           "the Big Picture the sign-in asked for is closed again");
}

void testThePlayerClosingTheWindowEndsIt() {
    Rig rig;
    rig.steam.answerPages([&rig](const std::string& page, const std::string&) -> json {
        rig.steam.removePage(page);
        return {{"state", "login"}};
    });
    SteamGridDbSignIn signIn{rig.steam.port(), quick};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        std::stop_token{},
        [](Step) {
        },
        error);
    expect(!key && error == "the SteamGridDB window was closed",
           "a closed window ends the sign-in");
    expect(rig.steam.closed().empty(), "a window the player closed is not closed again");
}

void testCancelClosesTheView() {
    Rig rig;
    rig.steam.answerPages([&rig](const std::string& page, const std::string&) -> json {
        rig.steam.putPage(page, openId);
        return {{"state", "login"}, {"clicked", true}};
    });
    std::stop_source cancel;
    SteamGridDbSignIn signIn{rig.steam.port(), quick};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        cancel.get_token(),
        [&cancel](Step step) {
            if (step == Step::WaitingForPlayer) {
                cancel.request_stop();
            }
        },
        error);
    expect(!key && error == "cancelled", "a cancel ends the sign-in");
    expect(rig.steam.closed() == std::vector<std::string>{"VIEW"}, "a cancelled view is closed");
}

void testTheTimeLimitClosesTheView() {
    Rig rig;
    rig.steam.answerPages([&rig](const std::string& page, const std::string&) -> json {
        rig.steam.putPage(page, openId);
        return {{"state", "login"}, {"clicked", true}};
    });
    SteamGridDbSignIn signIn{rig.steam.port(), SteamGridDbSignIn::Timing{10ms, 100ms}};
    std::string error;
    const std::optional<std::string> key = signIn.run(
        std::stop_token{},
        [](Step) {
        },
        error);
    expect(!key && contains(error, "not confirmed in time"), "an unconfirmed sign-in gives up");
    expect(rig.steam.closed() == std::vector<std::string>{"VIEW"}, "the abandoned view is closed");
}

void testNoSteamIsUnreachable() {
    std::uint16_t port = 0;
    {
        const FakeSteam gone{[](const std::string&) {
            return json(true);
        }};
        port = gone.port();
    }
    SteamGridDbSignIn signIn{port, quick};
    std::string error;
    expect(!signIn.reachable(error) && !error.empty(), "no Steam is not reachable");
    expect(!signIn.run(
               std::stop_token{},
               [](Step) {
               },
               error),
           "no Steam gives no key");
}

void testKeyShape() {
    expect(opensu::steam::isSteamGridDbKey(fakeKey), "32 hex digits are a key");
    expect(opensu::steam::isSteamGridDbKey("0123456789ABCDEF0123456789ABCDEF"),
           "capitals are a key");
    expect(!opensu::steam::isSteamGridDbKey("0123456789abcdef0123456789abcde"), "31 are not");
    expect(!opensu::steam::isSteamGridDbKey("0123456789abcdef0123456789abcdeg"),
           "a letter past f is not");
}

} // namespace

int main() try {
    try {
        testKeyShape();
        testSignsInAndReadsTheKey();
        testOwnSteamSignsInThroughBigPicture();
        testBigPictureAlreadyUpStaysUp();
        testCancelWhileBigPictureOpensClosesIt();
        testThePlayerClosingTheWindowEndsIt();
        testCancelClosesTheView();
        testTheTimeLimitClosesTheView();
        testNoSteamIsUnreachable();
        std::printf("steamgriddb sign-in: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}

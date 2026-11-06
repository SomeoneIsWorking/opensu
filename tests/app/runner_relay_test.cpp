// The relay through its shipping code: SDL events as the watch hears them, what it posts to a fake
// runner, and when it rests.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <nlohmann/json.hpp>

#include "app/runner_relay.hpp"
#include "gamepad/sdl_pads.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu;

int failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

struct Posted {
    std::string route;
    nlohmann::json body;
};

class FakeRunner {
  public:
    app::RunnerRelay::Send send() {
        return [this](const std::string& route, const std::string& body) {
            const std::lock_guard lock{mutex_};
            posts_.push_back(Posted{route, nlohmann::json::parse(body)});
        };
    }

    /// Waits for `count` more posts and takes them.
    std::vector<Posted> take(std::size_t count) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        for (;;) {
            {
                const std::lock_guard lock{mutex_};
                if (posts_.size() >= count || std::chrono::steady_clock::now() >= deadline) {
                    std::vector<Posted> taken;
                    taken.swap(posts_);
                    return taken;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    }

    /// Whatever came, once the relay had time to post it.
    std::vector<Posted> settled() {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        const std::lock_guard lock{mutex_};
        std::vector<Posted> taken;
        taken.swap(posts_);
        return taken;
    }

  private:
    std::mutex mutex_;
    std::vector<Posted> posts_;
};

constexpr touch::WindowSurface screen{.width = 1000,
                                      .height = 500,
                                      .safeX = 10.0F,
                                      .safeY = 0.0F,
                                      .safeWidth = 980.0F,
                                      .safeHeight = 500.0F,
                                      .dp = 2.0F};

SDL_Event padButton(SDL_GamepadButton button, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.which = 3;
    event.gbutton.button = static_cast<Uint8>(button);
    event.gbutton.down = down;
    return event;
}

SDL_Event padAxis(SDL_GamepadAxis axis, Sint16 value) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.which = 3;
    event.gaxis.axis = static_cast<Uint8>(axis);
    event.gaxis.value = value;
    return event;
}

SDL_Event keyEvent(SDL_Scancode scancode, bool down, bool repeat) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scancode;
    event.key.down = down;
    event.key.repeat = repeat;
    return event;
}

std::string nameOf(SDL_GamepadButton button) {
    return std::string{gamepad::name(gamepad::SdlPadState::buttonOf(button))};
}

bool holds(const nlohmann::json& body, const std::string& name) {
    const nlohmann::json& buttons = body["buttons"];
    return std::ranges::find(buttons, nlohmann::json(name)) != buttons.end();
}

void nothingBeforeTheGameShows() {
    FakeRunner runner;
    app::RunnerRelay relay{runner.send()};
    relay.hear(padButton(SDL_GAMEPAD_BUTTON_SOUTH, true));
    expect(!relay.isOpen() && !relay.forwarding(), "a closed relay forwards nothing");
    relay.open();
    relay.follow(false, false, 1, screen);
    relay.hear(padButton(SDL_GAMEPAD_BUTTON_SOUTH, true));
    expect(!relay.forwarding(), "the card keeps the buttons until the game shows");
    expect(runner.settled().empty(), "nothing is posted until the game shows");
}

void theRunnerLearnsTheShell() {
    FakeRunner runner;
    app::RunnerRelay relay{runner.send()};
    relay.open();
    relay.follow(true, false, 1, screen);
    expect(relay.forwarding(), "a game in front takes the input");
    const std::vector<Posted> posts = runner.take(3);
    expect(posts.size() == 3 && posts[0].route == "/host" && posts[0].body["pads"] == 1,
           "the pad count comes first");
    expect(posts.size() == 3 && posts[1].body["screen"]["safeX"] == 10.0 &&
               posts[1].body["screen"]["dp"] == 2.0,
           "then the screen");
    expect(posts.size() == 3 && posts[2].body["covered"] == false,
           "and last that the game is in front");
    relay.follow(true, false, 1, screen);
    expect(runner.settled().empty(), "what has not changed is not posted again");
}

void padsReachTheGame() {
    FakeRunner runner;
    app::RunnerRelay relay{runner.send()};
    relay.open();
    relay.follow(true, false, 1, screen);
    static_cast<void>(runner.take(3));

    relay.hear(padButton(SDL_GAMEPAD_BUTTON_SOUTH, true));
    std::vector<Posted> posts = runner.take(1);
    expect(posts.size() == 1 && posts[0].route == "/input" && posts[0].body["pad"] == "sdl:3" &&
               holds(posts[0].body, nameOf(SDL_GAMEPAD_BUTTON_SOUTH)),
           "a press holds its button");

    relay.hear(padButton(SDL_GAMEPAD_BUTTON_GUIDE, true));
    for (const Posted& post : runner.settled()) {
        expect(!holds(post.body, nameOf(SDL_GAMEPAD_BUTTON_GUIDE)), "Guide stays with the shell");
    }

    relay.hear(padAxis(SDL_GAMEPAD_AXIS_LEFTX, 32767));
    posts = runner.take(1);
    expect(!posts.empty() && posts.back().body["left"][0] == 1.0, "a stick reaches the game");
    expect(!posts.empty() && holds(posts.back().body, nameOf(SDL_GAMEPAD_BUTTON_SOUTH)),
           "the press stands while the stick moves");

    relay.hear(padButton(SDL_GAMEPAD_BUTTON_SOUTH, false));
    posts = runner.take(1);
    expect(!posts.empty() && !holds(posts.back().body, nameOf(SDL_GAMEPAD_BUTTON_SOUTH)),
           "a release lets go");

    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = 3;
    relay.hear(removed);
    posts = runner.take(1);
    expect(!posts.empty() && posts.back().body["leave"] == true, "a pad that goes leaves the game");
}

void aMenuRestsTheGame() {
    FakeRunner runner;
    app::RunnerRelay relay{runner.send()};
    relay.open();
    relay.follow(true, false, 1, screen);
    static_cast<void>(runner.take(3));
    relay.hear(padButton(SDL_GAMEPAD_BUTTON_EAST, true));
    static_cast<void>(runner.take(1));

    relay.follow(true, true, 1, screen);
    expect(!relay.forwarding(), "the menu takes the input");
    std::vector<Posted> posts = runner.take(2);
    expect(posts.size() == 2 && posts[0].body["covered"] == true, "the game is told it is covered");
    expect(posts.size() == 2 && posts[1].body["buttons"].empty(), "and every pad is let go");

    relay.hear(padButton(SDL_GAMEPAD_BUTTON_SOUTH, true));
    expect(runner.settled().empty(), "a press in the menu is the menu's");

    relay.follow(true, false, 1, screen);
    posts = runner.take(2);
    expect(posts.size() == 2 && posts[0].body["covered"] == false, "back in front");
    expect(posts.size() == 2 && holds(posts[1].body, nameOf(SDL_GAMEPAD_BUTTON_EAST)),
           "the pad stands as it is held now");

    SDL_Event lost{};
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    relay.hear(lost);
    expect(!relay.forwarding(), "a window without the input forwards none");
    SDL_Event gained{};
    gained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    relay.hear(gained);
    expect(relay.forwarding(), "it comes back with the focus");
}

void keysAndFingers() {
    FakeRunner runner;
    app::RunnerRelay relay{runner.send()};
    relay.open();
    relay.follow(true, false, 0, screen);
    static_cast<void>(runner.take(3));

    relay.hear(keyEvent(SDL_SCANCODE_X, true, false));
    relay.hear(keyEvent(SDL_SCANCODE_X, true, true));
    relay.hear(keyEvent(SDL_SCANCODE_AC_BACK, true, false));
    const std::vector<Posted> keys = runner.settled();
    expect(keys.size() == 1 && keys[0].route == "/host" &&
               keys[0].body["key"]["scancode"] == static_cast<int>(SDL_SCANCODE_X) &&
               keys[0].body["key"]["down"] == true,
           "one press is one key; a repeat and Back are not the game's");

    SDL_Event finger{};
    finger.type = SDL_EVENT_FINGER_DOWN;
    finger.tfinger.fingerID = 7;
    finger.tfinger.x = 0.5F;
    finger.tfinger.y = 0.5F;
    relay.hear(finger);
    const std::vector<Posted> touches = runner.take(1);
    expect(touches.size() == 1 && touches[0].route == "/touch" && touches[0].body["x"] == 500.0 &&
               touches[0].body["y"] == 250.0 && touches[0].body["phase"] == "down",
           "a finger comes in the surface's pixels");

    relay.close();
    relay.hear(keyEvent(SDL_SCANCODE_Z, true, false));
    expect(!relay.forwarding() && runner.settled().empty(), "a closed relay is silent");
}

} // namespace

int main() try {
    nothingBeforeTheGameShows();
    theRunnerLearnsTheShell();
    padsReachTheGame();
    aMenuRestsTheGame();
    keysAndFingers();
    if (failures != 0) {
        return 1;
    }
    std::printf("runner_relay: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

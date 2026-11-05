// A netplay session over an in-memory link: both sides see the same input on the same frame, a
// lost message costs nothing, a machine waits for the other, and the session ends when the
// machines differ, when one leaves and when one goes quiet.
#include "netplay/session.hpp"

#include <chrono>
#include <cstdio>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::netplay::PlayerInput;
using opensu::netplay::Session;
using opensu::netplay::SessionConfig;
using opensu::netplay::SessionEnd;
using opensu::netplay::Transport;
using opensu::test::expect;

/// One end of a link: what it sends lands in the other's queue, every `dropEvery`th one lost.
class Link final : public Transport {
  public:
    Link* other{nullptr};
    std::deque<std::string> arrived;
    int dropEvery{0};
    int sent{0};

    void send(std::string_view message) override {
        ++sent;
        if (dropEvery != 0 && sent % dropEvery == 0) {
            return;
        }
        other->arrived.emplace_back(message);
    }

    std::optional<std::string> receive(std::chrono::milliseconds /*wait*/) override {
        if (arrived.empty()) {
            return std::nullopt;
        }
        std::string message = std::move(arrived.front());
        arrived.pop_front();
        return message;
    }
};

struct Pair {
    Link one;
    Link two;

    Pair() {
        one.other = &two;
        two.other = &one;
    }
};

PlayerInput pressed(std::uint16_t buttons) {
    PlayerInput input;
    input.buttons = buttons;
    input.sticks[2] = -300;
    return input;
}

/// Runs both sides for `frames`, checking they read the same input, each frame's input being its
/// frame number on slot 0 and twice it on slot 1.
void runBoth(Session& a, Session& b, std::uint64_t frames) {
    for (std::uint64_t at = 0; at < frames; ++at) {
        const auto value = static_cast<std::uint16_t>(a.frame() + 1);
        a.submit(pressed(value));
        b.submit(pressed(static_cast<std::uint16_t>(value * 2)));
        bool aReady = false;
        bool bReady = false;
        for (int tries = 0; tries < 50 && !(aReady && bReady); ++tries) {
            aReady = a.ready(0ms);
            bReady = b.ready(0ms);
            std::this_thread::sleep_for(1ms);
        }
        expect(aReady && bReady, "both sides get every frame's input");
        expect(a.input(0) == b.input(0) && a.input(1) == b.input(1),
               "both sides read the same input on the same frame");
        a.advance();
        b.advance();
    }
}

void sameInputOnSameFrame() {
    Pair link;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 2}};
    Session b{link.two, {.players = 2, .slot = 1, .delay = 2}};
    a.submit(pressed(1));
    b.submit(pressed(2));
    expect(a.ready(0ms) && b.ready(0ms), "the delayed frames need nobody's input");
    expect(a.input(0) == PlayerInput{} && b.input(1) == PlayerInput{},
           "a frame before the delay is neutral");
    a.advance();
    b.advance();
    a.submit(pressed(3));
    b.submit(pressed(4));
    expect(a.ready(0ms) && b.ready(0ms), "the second delayed frame runs");
    a.advance();
    b.advance();
    a.submit(pressed(5));
    b.submit(pressed(6));
    expect(a.ready(0ms) && b.ready(0ms), "input read two frames ago is in");
    expect(a.input(0) == pressed(1) && a.input(1) == pressed(2) && b.input(0) == pressed(1) &&
               b.input(1) == pressed(2),
           "it takes effect `delay` frames after it was read, on both machines");
}

void lossCostsNothing() {
    Pair link;
    link.one.dropEvery = 3;
    link.two.dropEvery = 4;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 2}};
    Session b{link.two, {.players = 2, .slot = 1, .delay = 2}};
    runBoth(a, b, 300);
    expect(a.ended() == SessionEnd::None && b.ended() == SessionEnd::None, "a lossy link plays on");
}

void waitsForTheOther() {
    Pair link;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 1}};
    a.submit(pressed(1));
    expect(a.ready(0ms), "the delayed frame runs alone");
    a.advance();
    a.submit(pressed(2));
    expect(!a.ready(0ms) && a.stalls() == 1, "the next one waits for the other machine");
    expect(!a.ready(0ms) && a.stalls() == 1, "one stall however long it lasts");
    Session b{link.two, {.players = 2, .slot = 1, .delay = 1}};
    b.submit(pressed(7));
    expect(a.ready(0ms) && a.input(1) == pressed(7), "and runs when its input arrives");
}

void differingGamesEndIt() {
    Pair link;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 2}};
    Session b{link.two, {.players = 2, .slot = 1, .delay = 2}};
    runBoth(a, b, Session::checkInterval);
    expect(a.checkDue() && b.checkDue(), "the frame to compare comes round");
    a.check(0xABCD);
    b.check(0xABCD);
    expect(a.ready(0ms) || a.ended() == SessionEnd::None, "the same checksum plays on");
    runBoth(a, b, Session::checkInterval);
    a.check(1);
    b.check(2);
    static_cast<void>(a.ready(0ms));
    static_cast<void>(b.ready(0ms));
    expect(a.ended() == SessionEnd::Desync && b.ended() == SessionEnd::Desync,
           "a differing checksum ends it on both sides");
}

void leavingEndsIt() {
    Pair link;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 2}};
    Session b{link.two, {.players = 2, .slot = 1, .delay = 2}};
    a.leave();
    static_cast<void>(b.ready(0ms));
    expect(b.ended() == SessionEnd::Left, "a player leaving ends it for the other");
}

void quietEndsIt() {
    Pair link;
    Session a{link.one, {.players = 2, .slot = 0, .delay = 0, .quiet = 1ms, .joining = 1ms}};
    a.submit(pressed(1));
    std::this_thread::sleep_for(5ms);
    expect(!a.ready(0ms) && a.ended() == SessionEnd::Quiet, "nothing heard for too long ends it");
}

} // namespace

int main() try {
    sameInputOnSameFrame();
    lossCostsNothing();
    waitsForTheOther();
    differingGamesEndIt();
    leavingEndsIt();
    quietEndsIt();
    std::printf("netplay session: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// A public game through a lab relay on loopback: listed by its name alone, its code handed sealed
// to the friend who asks, then taken down; a second hosting unlinkable to the first; and the
// settings' relay list as the places read it.
#include "online/lobbies.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>

#include "check.hpp"
#include "lucent/http.h"
#include "lucent/p2p/nostr_relay.h"
#include "lucent/websocket.h"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using namespace opensu::online;
using opensu::test::expect;

/// Waits up to `seconds` for `done`.
bool within(std::chrono::seconds seconds, const std::function<bool()>& done) {
    const auto until = std::chrono::steady_clock::now() + seconds;
    while (std::chrono::steady_clock::now() < until) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(50ms);
    }
    return done();
}

/// The listing event of the game named `title`, as a reader of the relay sees it.
lucent::p2p::nostr::Event listingOf(lucent::p2p::nostr::RelayStore& store,
                                    const std::string& title) {
    for (lucent::p2p::nostr::Event& event :
         store.query(lucent::json::Array{lucent::json::Value{lucent::json::Object{}}})) {
        if (event.content.find("\"title\":\"" + title + "\"") != std::string::npos) {
            return event;
        }
    }
    opensu::test::fail("the game's listing is on the relay");
}

/// The contact key a listing's content carries.
std::string contactOf(const lucent::p2p::nostr::Event& listing) {
    const std::size_t at = listing.content.find("\"contact\":\"");
    expect(at != std::string::npos, "a listing carries a contact key");
    return listing.content.substr(at + 11, 64);
}

void placesReadTheSettings() {
    const LobbyPlaces both = LobbyPlaces::of(" wss://a.example , ,wss://b.example", " ");
    expect(both.relays.size() == 2 && both.relays[0] == "wss://a.example" &&
               both.relays[1] == "wss://b.example" && both.server.empty(),
           "the relay list splits on commas and drops blanks");
    expect(LobbyPlaces::of("", "").relays.empty(), "no relays means Lucent's defaults");
    LobbyFinder bad{LobbyPlaces::of("", "not a url")};
    expect(within(5s,
                  [&] {
                      return bad.view().step == LobbyFinder::Step::Failed;
                  }),
           "a server that is no URL fails the list");
}

void aFriendIsSeatedFromTheList() {
    lucent::p2p::nostr::RelayStore store;
    lucent::http::Server server{lucent::http::ServerOptions{.max_connections = 32},
                                [&store](const lucent::http::Request& request) {
                                    return lucent::http::websocket_upgrade(
                                        request, [&store](lucent::http::WebSocket& socket) {
                                            store.serve(socket);
                                        });
                                }};
    expect(server.start(), "the lab relay starts");
    const LobbyPlaces places =
        LobbyPlaces::of("ws://127.0.0.1:" + std::to_string(server.port()), "");
    lucent::p2p::nostr::Event first;
    {
        PublicLobby lobby{places, "Saturday night", "OSU-THE-CODE"};
        expect(within(10s,
                      [&] {
                          return lobby.view().listed;
                      }),
               "the game is listed");
        first = listingOf(store, "Saturday night");
        for (const lucent::p2p::nostr::Event& event :
             store.query(lucent::json::Array{lucent::json::Value{lucent::json::Object{}}})) {
            expect(event.content.find("OSU-THE-CODE") == std::string::npos,
                   "no event holds the code");
        }
        LobbyFinder finder{places};
        expect(within(10s,
                      [&] {
                          return finder.view().step == LobbyFinder::Step::Listed;
                      }),
               "the list comes back");
        const LobbyFinder::View listed = finder.view();
        expect(listed.games.size() == 1 && listed.games[0].title == "Saturday night" &&
                   listed.games[0].players == 1 && listed.games[0].capacity == 2,
               "the game shows by its name and seats");
        finder.ask(0);
        expect(within(20s,
                      [&] {
                          return finder.view().step == LobbyFinder::Step::Invited;
                      }),
               "the host answers");
        expect(finder.view().invite == "OSU-THE-CODE", "the answer is the host's code");
        expect(within(10s,
                      [&] {
                          return lobby.view().seated && !lobby.view().listed;
                      }),
               "the host is seated and the listing down");
    }
    LobbyFinder after{places};
    expect(within(10s,
                  [&] {
                      return after.view().step == LobbyFinder::Step::Listed;
                  }),
           "the list comes back after");
    expect(after.view().games.empty(), "a seated game is no longer listed");
    {
        PublicLobby again{places, "Sunday", "OSU-ANOTHER-CODE"};
        expect(within(10s,
                      [&] {
                          return again.view().listed;
                      }),
               "a second hosting is listed");
        const lucent::p2p::nostr::Event second = listingOf(store, "Sunday");
        expect(second.pubkey != first.pubkey, "each hosting signs with a key of its own");
        expect(contactOf(second) != contactOf(first), "each hosting has a contact key of its own");
    }
    server.stop();
}

} // namespace

int main() try {
    placesReadTheSettings();
    aFriendIsSeatedFromTheList();
    std::printf("lobbies: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// LAN pairing and remote libraries between openSUs in one process: real listeners on loopback,
// the discovery each node would get from Avahi stood in by a shared directory of announcements.
// Pairing shows the same code on both sides and leaves both books holding the same secret; each
// then lists the other's installed games and covers; an unpaired openSU learns only the name;
// signed requests that are forged, late or repeated are refused, and their bodies sealed;
// forgetting revokes both sides. Saves shared both ways follow the player: pulled before a game,
// pushed after, a conflict left for the player to pick, every replaced save backed up.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "lan/dns_sd.hpp"
#include "lan/host_pairing.hpp"
#include "lan/lan_listener.hpp"
#include "lan/lan_node.hpp"
#include "lan/pairing_keys.hpp"
#include "lan/peer_book.hpp"
#include "lan/peer_client.hpp"
#include "lan/peer_crypto.hpp"
#include "lan/remote_library.hpp"
#include "lan/request_signature.hpp"
#include "lan/save_bundle.hpp"
#include "lan/save_sync.hpp"
#include "lan/shared_library.hpp"
#include "lan/wire.hpp"
#include "net/web_client.hpp"
#include "secrets/secret.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using lan::GuestPairing;

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

/// What `held` holds, failing with `what` when it holds nothing.
template <typename T> T must(std::optional<T> held, const char* what) {
    if (!held) {
        fail(what);
    }
    return std::move(*held);
}

bool waitFor(const std::function<bool()>& holds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (std::chrono::steady_clock::now() < until) {
        if (holds()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    return holds();
}

/// Every node's announcement, as Avahi would resolve them on loopback.
class Directory {
  public:
    void announce(const lan::Announcement& self) {
        const std::lock_guard lock{mutex_};
        announced_[self.id] = lan::Sighting{self.id, self.name, "127.0.0.1", self.port};
    }
    void withdraw(const std::string& id) {
        const std::lock_guard lock{mutex_};
        announced_.erase(id);
    }
    [[nodiscard]] std::vector<lan::Sighting> except(const std::string& id) const {
        const std::lock_guard lock{mutex_};
        std::vector<lan::Sighting> seen;
        for (const auto& [peer, sighting] : announced_) {
            if (peer != id) {
                seen.push_back(sighting);
            }
        }
        return seen;
    }

  private:
    mutable std::mutex mutex_;
    std::map<std::string, lan::Sighting> announced_;
};

class DirectoryDiscovery final : public lan::ServiceDiscovery {
  public:
    DirectoryDiscovery(Directory& directory, lan::Announcement self)
        : directory_{directory}, self_{std::move(self)} {
        directory_.announce(self_);
    }
    ~DirectoryDiscovery() override {
        directory_.withdraw(self_.id);
    }
    DirectoryDiscovery(const DirectoryDiscovery&) = delete;
    DirectoryDiscovery& operator=(const DirectoryDiscovery&) = delete;
    DirectoryDiscovery(DirectoryDiscovery&&) = delete;
    DirectoryDiscovery& operator=(DirectoryDiscovery&&) = delete;

    [[nodiscard]] std::vector<lan::Sighting> sightings() const override {
        return directory_.except(self_.id);
    }
    [[nodiscard]] std::string announcedName() const override {
        return self_.name;
    }

  private:
    Directory& directory_;
    lan::Announcement self_;
};

/// One openSU: its node, its remote games, how often they changed, and where it keeps saves.
struct Node {
    Node(const fs::path& root, const std::string& name, Directory& directory)
        : remote{std::make_shared<lan::RemoteLibrary>()}, saves{root / name / "saves"},
          state{root / name / "state"},
          node{lan::LanNode::Setup{
              .bookFile = root / name / "lan.json",
              .secrets = secrets::SecretStore{root / name / "secrets", false},
              .coverCache = root / name / "covers",
              .name = name,
              .port = 0,
              .remote = remote,
              .changed =
                  [this] {
                      ++changes;
                  },
              .saves = lan::SaveSync::Setup{.versionsFile = state / "lan-saves.json",
                                            .backups = state,
                                            .staging = state / "lan-incoming",
                                            .runnerSaves = saves,
                                            .inUse =
                                                [this](const library::Game&,
                                                       const library::saves::EmulatorSaves&) {
                                                    const std::lock_guard lock{mutex};
                                                    return busy;
                                                },
                                            .changed =
                                                [this] {
                                                    ++savesChanged;
                                                }},
              .play = [this](const lan::Peer& peer, const std::string& game,
                             const lan::StreamKeys& keys,
                             std::string& refusal) -> std::optional<std::uint16_t> {
                  const std::lock_guard lock{mutex};
                  if (!refusePlay.empty()) {
                      refusal = refusePlay;
                      return std::nullopt;
                  }
                  played = peer.name + " " + game;
                  playKeys = keys;
                  return streamPort;
              },
              .discovery =
                  [&directory](const lan::Announcement& self, std::string& /*error*/) {
                      return std::make_unique<DirectoryDiscovery>(directory, self);
                  }}} {
    }

    /// Why this side's saves cannot be replaced now; empty when they can.
    void setBusy(std::string why) {
        const std::lock_guard lock{mutex};
        busy = std::move(why);
    }

    [[nodiscard]] lan::Endpoint endpoint() const {
        return lan::Endpoint{"127.0.0.1", node.status().port};
    }

    std::shared_ptr<lan::RemoteLibrary> remote;
    std::atomic<int> changes{0};
    std::atomic<int> savesChanged{0};
    std::mutex mutex;
    std::string busy;
    /// What `play` answers: the refusal, else the port; and what it was last asked.
    std::string refusePlay;
    static constexpr std::uint16_t streamPort = 4242;
    std::string played;
    lan::StreamKeys playKeys;
    fs::path saves;
    fs::path state;
    lan::LanNode node;
};

/// A catalog with the built-in mGBA core, which keeps Game Boy saves in the runner's folder.
library::roms::EmulatorCatalog runnerCatalog() {
    library::roms::EmulatorEntry entry;
    entry.id = "mgba-core";
    entry.name = "mGBA (built-in)";
    entry.systems = {"gb"};
    entry.core = "mgba_libretro";
    return library::roms::EmulatorCatalog{{entry}};
}

/// The same Game Boy ROM, installed on each openSU, on the built-in runner.
library::Game runnerGame() {
    library::Game game;
    game.id = "rom:/r/Test.gb";
    game.source = library::Source::Rom;
    game.sourceId = "gb";
    game.title = "Test";
    game.file = "/r/Test.gb";
    game.installed = true;
    library::EmulatorOption option;
    option.name = "mGBA (built-in)";
    option.id = "mgba-core";
    option.installed = true;
    game.emulatorOptions = {option};
    game.emulator = option.name;
    return game;
}

library::Game installedGame(const std::string& id, const std::string& title,
                            const fs::path& cover) {
    library::Game game;
    game.id = id;
    game.source = library::Source::Own;
    game.title = title;
    game.installed = true;
    game.artwork = cover;
    return game;
}

void writeFile(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary};
    out << bytes;
}

std::string readFile(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// `guest` pairs with `host` and the player at the host answers `accept`; the phase it ends in.
GuestPairing::Phase pairUp(Node& guest, Node& host, bool accept) {
    std::string refusal;
    expect(guest.node.pair(host.node.id(), refusal), "a found openSU can be asked to pair");
    expect(waitFor([&host] {
               return host.node.question().has_value();
           }),
           "the host asks its player");
    const lan::HostPairing::Question question = must(host.node.question(), "a question");
    expect(question.guest.id == guest.node.id(), "it names the guest");
    expect(waitFor([&guest] {
               const auto state = guest.node.asking();
               return state && state->phase == GuestPairing::Phase::Waiting;
           }),
           "the guest waits for the host's player");
    expect(must(guest.node.asking(), "a pairing").code == question.code &&
               question.code.size() == 7,
           "both screens show the same six-digit code");
    std::string error;
    expect(host.node.answer(question.guest.id, accept, error), "the answer is kept");
    expect(waitFor([&guest] {
               const auto state = guest.node.asking();
               return state && state->phase != GuestPairing::Phase::Waiting;
           }),
           "the guest hears the answer");
    const GuestPairing::Phase phase = must(guest.node.asking(), "a pairing").phase;
    guest.node.endAsking();
    return phase;
}

void pairedNodesListEachOthersGames(const fs::path& root) {
    Directory directory;
    Node host{root, "Living room", directory};
    Node guest{root, "Desk", directory};
    Node stranger{root, "Stranger", directory};
    const fs::path cover = root / "art" / "cover.png";
    writeFile(cover, "not really a png");
    library::Game uninstalled = installedGame("own:3", "Gone", {});
    uninstalled.installed = false;
    host.node.share({installedGame("own:1", "Shared game", cover), uninstalled}, runnerCatalog());
    guest.node.share({installedGame("own:9", "Guest game", {})}, runnerCatalog());

    expect(waitFor([&guest] {
               return guest.node.sightings().size() == 2;
           }),
           "each openSU finds the others");
    expect(guest.node.paired().empty() && guest.remote->games().empty(),
           "nothing is listed before pairing");

    expect(pairUp(stranger, host, false) == GuestPairing::Phase::Declined,
           "a declined pairing says so");
    expect(stranger.node.paired().empty() && host.node.paired().empty(),
           "and neither side keeps the other");

    expect(pairUp(guest, host, true) == GuestPairing::Phase::Paired, "an accepted pairing pairs");
    expect(host.node.paired().size() == 1 && host.node.paired().front().name == "Desk" &&
               guest.node.paired().size() == 1 && guest.node.paired().front().name == "Living room",
           "both books keep the other");

    expect(waitFor([&guest] {
               return guest.remote->games().size() == 1;
           }),
           "the guest lists the host's installed game");
    const library::Game listed = guest.remote->games().front();
    expect(listed.source == library::Source::Remote && listed.title == "Shared game" &&
               listed.hostName == "Living room" && listed.hostId == host.node.id() &&
               listed.sourceId == "own:1" && listed.launch.empty(),
           "marked with its host and its id there");
    expect(!listed.artwork.empty() && readFile(listed.artwork) == "not really a png" &&
               listed.artwork.extension() == ".png",
           "its cover is the host's, cached here");
    expect(guest.changes > 0, "the guest was told its remote games changed");

    std::string playError;
    const std::optional<lan::LanNode::PlayOffer> offer =
        guest.node.askToPlay(host.node.id(), listed.sourceId, playError);
    expect(offer && offer->stream.port == Node::streamPort && offer->stream.address == "127.0.0.1",
           "the host agrees to stream its game and says where");
    {
        const std::lock_guard lock{host.mutex};
        expect(host.played == "Desk own:1", "the host is asked by the guest for that game");
        expect(offer && offer->keys.host == host.playKeys.host &&
                   offer->keys.guest == host.playKeys.guest && offer->keys.host.size() == 32 &&
                   offer->keys.host != offer->keys.guest,
               "both sides hold one pair of session keys, one for each way");
        host.refusePlay = "a game is already running there";
    }
    expect(!guest.node.askToPlay(host.node.id(), listed.sourceId, playError) &&
               playError.find("a game is already running there") != std::string::npos,
           "a host that cannot stream says why");
    expect(!guest.node.askToPlay(host.node.id(), "own:2", playError),
           "a game the host does not list is not streamed");
    {
        const std::lock_guard lock{host.mutex};
        host.refusePlay.clear();
    }
    const std::optional<lan::LanNode::PlayOffer> again =
        guest.node.askToPlay(host.node.id(), listed.sourceId, playError);
    expect(again && offer && again->keys.host != offer->keys.host,
           "each session has keys of its own");
    expect(waitFor([&host] {
               return host.remote->games().size() == 1 &&
                      host.remote->games().front().title == "Guest game";
           }),
           "the pairing works both ways: the host lists the guest's game");

    net::WebClient web;
    std::string error;
    const lan::PeerClient toHost{web, host.endpoint()};
    const auto hello =
        web.get("http://127.0.0.1:" + std::to_string(host.endpoint().port) + "/lan/hello", error);
    const auto said = hello ? lan::wire::readHello(*hello) : std::nullopt;
    expect(said && said->id == host.node.id() && said->name == "Living room",
           "anyone may ask its name");
    const lan::crypto::Bytes guessed = lan::crypto::randomBytes(lan::crypto::keyBytes);
    const std::string strangerId = stranger.node.id();
    const auto refused = toHost.call("GET", "/lan/library", {},
                                     lan::PeerClient::Signer{strangerId, guessed}, 1 << 20, error);
    expect(!refused && error.find("not paired") != std::string::npos,
           "an unpaired openSU is refused the library");
    const std::string guestId = guest.node.id();
    const auto forged = toHost.call("GET", "/lan/library", {},
                                    lan::PeerClient::Signer{guestId, guessed}, 1 << 20, error);
    expect(!forged, "a request in the guest's name without its secret is refused");

    expect(host.node.forget(guest.node.id(), error), "the host forgets the guest");
    expect(host.node.paired().empty(), "the host's book drops it");
    expect(waitFor([&guest] {
               return guest.node.paired().empty();
           }),
           "the guest is told and forgets the host");
    expect(waitFor([&guest] {
               return guest.remote->games().empty();
           }),
           "the host's games leave the guest's library");
    expect(waitFor([&listed] {
               return !fs::exists(listed.artwork.parent_path());
           }),
           "and their covers its cache");
    expect(waitFor([&host] {
               return host.remote->games().empty();
           }),
           "and the guest's leave the host's");
}

/// A bundle from another openSU names files inside their slots only.
void bundlesStayInTheirSlots() {
    using library::saves::SlotKind;
    const auto file = [](std::string path) {
        return lan::BundleFile{
            .kind = SlotKind::Memory, .path = std::move(path), .bytes = "x", .modified = {}};
    };
    const lan::SaveBundle saves{{file("slot/one.sav"), file("two.sav")}};
    const auto read = lan::readBundle(lan::write(saves));
    expect(read && read->hash() == saves.hash() && read->bytes() == 2,
           "a bundle reads back as written");
    for (const char* outside : {"../x", "a/../../x", "/etc/x", "a\\x", "c:x", "a//x", "./x"}) {
        expect(!lan::readBundle(lan::write(lan::SaveBundle{{file(outside)}})),
               "a path outside its slot is refused");
    }
    expect(!lan::readBundle(lan::write(lan::SaveBundle{{file("a"), file("a")}})),
           "and so are two files at one path");
    expect(!lan::readBundle("99\n{\"files\":[]}"), "and a header longer than the bundle");
}

/// The standing of `node`'s saves of `game` with its one sharing peer, once a look settles.
std::optional<lan::PeerSaves::Standing> standingOf(Node& node, const library::Game& game) {
    const std::vector<lan::PeerSaves> standings = node.node.saves().standings(game.id);
    if (standings.size() != 1) {
        return std::nullopt;
    }
    return standings.front().standing;
}

/// Looks at `game` on `node` until its saves stand as `expected` with the peer.
bool looksLike(Node& node, const library::Game& game, lan::PeerSaves::Standing expected) {
    return waitFor([&node, &game, expected] {
        if (node.node.saves().busy(game.id)) {
            return false;
        }
        node.node.saves().look(game);
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        return standingOf(node, game) == expected;
    });
}

/// Whether any file under `folder` holds exactly `bytes`.
bool holdsCopy(const fs::path& folder, const std::string& bytes) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it{folder, ec}, end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file() && readFile(it->path()) == bytes) {
            return true;
        }
    }
    return false;
}

void sharedSavesFollowThePlayer(const fs::path& root) {
    using Standing = lan::PeerSaves::Standing;
    Directory directory;
    Node host{root, "Den", directory};
    Node guest{root, "Attic", directory};
    const library::Game game = runnerGame();
    host.node.share({game}, runnerCatalog());
    guest.node.share({game}, runnerCatalog());
    const fs::path hostSave = host.saves / "gb" / "Test.srm";
    const fs::path guestSave = guest.saves / "gb" / "Test.srm";
    expect(waitFor([&guest] {
               return guest.node.sightings().size() == 1;
           }),
           "the two openSUs find each other");
    expect(pairUp(guest, host, true) == GuestPairing::Phase::Paired, "they pair");

    writeFile(hostSave, "saved on the den");
    const std::uint64_t unshared = guest.node.saves().prepare(game);
    expect(waitFor([&guest, unshared] {
               return guest.node.saves().prepared(unshared);
           }),
           "a launch is let go at once while no peer shares saves");
    expect(!fs::exists(guestSave) && guest.node.saves().standings(game.id).empty(),
           "and nothing is taken or shown");

    std::string error;
    expect(guest.node.shareSaves(host.node.id(), true, error),
           "the attic shares saves with the den");
    expect(guest.node.saves().sharing(game), "so the game's saves are shared");
    expect(looksLike(guest, game, Standing::NotShared),
           "while the den does not share back, it says so");

    expect(host.node.shareSaves(guest.node.id(), true, error), "the den shares back");
    expect(looksLike(guest, game, Standing::NewerThere), "the den's save is newer");
    const std::uint64_t ticket = guest.node.saves().prepare(game);
    expect(waitFor([&guest, ticket] {
               return guest.node.saves().prepared(ticket);
           }),
           "before the game starts here, its saves are synced");
    expect(readFile(guestSave) == "saved on the den",
           "a save made on the den is here before the game starts");
    expect(standingOf(guest, game) == Standing::InSync, "and both are in sync");

    writeFile(guestSave, "played in the attic");
    guest.node.saves().sync(game);
    expect(waitFor([&hostSave] {
               return readFile(hostSave) == "played in the attic";
           }),
           "a change here goes to the den after the game ends");
    expect(holdsCopy(host.state, "saved on the den"), "the den's save it replaced is backed up");
    expect(looksLike(guest, game, Standing::InSync) && looksLike(host, game, Standing::InSync),
           "and both say they are in sync");

    writeFile(hostSave, "changed on the den");
    writeFile(guestSave, "changed in the attic");
    expect(looksLike(guest, game, Standing::Conflict) && looksLike(host, game, Standing::Conflict),
           "a save changed on both sides is a conflict on both");
    guest.node.saves().sync(game);
    expect(looksLike(guest, game, Standing::Conflict) &&
               readFile(hostSave) == "changed on the den" &&
               readFile(guestSave) == "changed in the attic",
           "which a sync leaves alone");

    host.setBusy("Close mGBA first: it is running");
    guest.node.saves().keep(game, host.node.id(), false);
    expect(waitFor([&host, &game] {
               const auto standings = host.node.saves().standings(game.id);
               return !host.node.saves().busy(game.id) && standings.size() == 1 &&
                      standings.front().standing == Standing::Failed &&
                      standings.front().problem.find("running") != std::string::npos;
           }),
           "keeping the attic's is refused on the den while its emulator runs");
    expect(readFile(hostSave) == "changed on the den", "and the den's save is untouched");

    host.setBusy({});
    host.node.saves().keep(game, guest.node.id(), true);
    expect(waitFor([&hostSave] {
               return readFile(hostSave) == "changed in the attic";
           }),
           "once it closed, the player there keeps the attic's");
    expect(holdsCopy(host.state, "changed on the den"), "the save it replaced is backed up");
    expect(looksLike(host, game, Standing::InSync) && looksLike(guest, game, Standing::InSync),
           "and the two are in sync again");

    expect(guest.node.shareSaves(host.node.id(), false, error), "the attic stops sharing");
    expect(looksLike(host, game, Standing::NotShared), "and the den hears it no longer shares");
}

/// The signed routes, asked directly of a listener.
void signedRequestsAreCheckedOnce(const fs::path& root) {
    lan::PeerBook book{root / "signed" / "lan.json",
                       secrets::SecretStore{root / "signed" / "secrets", false}};
    lan::HostPairing pairing{book.id()};
    lan::SharedLibrary library;
    library.share({installedGame("own:1", "Shared game", {})});
    const net::WebClient web;
    lan::SaveSync saves{
        lan::SaveSync::Setup{.versionsFile = root / "signed" / "lan-saves.json",
                             .backups = root / "signed",
                             .staging = root / "signed" / "incoming",
                             .runnerSaves = root / "signed" / "saves",
                             .inUse =
                                 [](const library::Game&, const library::saves::EmulatorSaves&) {
                                     return std::string{};
                                 },
                             .changed =
                                 [] {
                                 }},
        book, web, [] {
            return std::vector<lan::Sighting>{};
        }};
    lan::LanListener listener{lan::LanListener::Sources{.book = book,
                                                        .pairing = pairing,
                                                        .library = library,
                                                        .saves = saves,
                                                        .name =
                                                            [] {
                                                                return std::string{"Host"};
                                                            },
                                                        .unpaired =
                                                            [](const lan::Peer&) {
                                                            },
                                                        .play = {}},
                              0};
    const lan::crypto::Bytes secret = lan::crypto::randomBytes(lan::crypto::keyBytes);
    const std::string peer = secrets::hexText(lan::crypto::randomBytes(lan::peerIdBytes));
    std::string error;
    expect(book.remember(lan::Peer{peer, "Guest"}, secret, error), "the guest is paired");

    const auto request = [&peer](const lan::crypto::Bytes& key, std::int64_t time,
                                 const std::string& nonce) {
        lucent::http::Request asked{"GET", "/lan/library", {}, {}};
        asked.headers = {{std::string{lan::peerHeader}, peer},
                         {std::string{lan::timeHeader}, std::to_string(time)},
                         {std::string{lan::nonceHeader}, nonce},
                         {std::string{lan::signatureHeader},
                          lan::requestSignature(
                              key, lan::RequestFacts{"GET", "/lan/library", time, nonce, {}})}};
        return asked;
    };
    const std::int64_t now = lan::unixSeconds(std::chrono::system_clock::now());
    const std::string nonce = secrets::hexText(lan::crypto::randomBytes(lan::crypto::nonceBytes));

    const lucent::http::Response answer = listener.handle(request(secret, now, nonce));
    expect(answer.status == 200, "a signed request is answered");
    expect(answer.body.find("Shared game") == std::string::npos,
           "the answer does not cross the LAN in clear");
    const lan::RequestFacts facts{"GET", "/lan/library", now, nonce, {}};
    const lan::crypto::Bytes other = lan::crypto::randomBytes(lan::crypto::keyBytes);
    expect(!lan::openBody(other, facts, lan::Direction::Answer, 200, answer.body),
           "it opens with no other secret");
    const std::string otherNonce =
        secrets::hexText(lan::crypto::randomBytes(lan::crypto::nonceBytes));
    const lan::RequestFacts otherRequest{"GET", "/lan/library", now, otherNonce, {}};
    expect(!lan::openBody(secret, otherRequest, lan::Direction::Answer, 200, answer.body),
           "nor as the answer to another request");
    const auto opened = lan::openBody(secret, facts, lan::Direction::Answer, 200, answer.body);
    const auto listing = opened ? lan::wire::readListing(*opened) : std::nullopt;
    expect(listing && listing->size() == 1 && listing->front().title == "Shared game",
           "sealed with the pairing's secret for this request, it holds the shared listing");

    expect(listener.handle(request(secret, now, nonce)).status == 403, "a repeat is refused");
    const std::string fresh = secrets::hexText(lan::crypto::randomBytes(lan::crypto::nonceBytes));
    expect(listener.handle(request(secret, now - 3600, fresh)).status == 403,
           "a request an hour late is refused");
    const std::string another = secrets::hexText(lan::crypto::randomBytes(lan::crypto::nonceBytes));
    expect(listener.handle(request(other, now, another)).status == 403,
           "a request signed with another secret is refused");
    expect(listener.handle(lucent::http::Request{"GET", "/lan/library", {}, {}}).status == 403,
           "an unsigned one too");
    expect(listener.handle(lucent::http::Request{"GET", "/lan/cover/../../etc", {}, {}}).status ==
               403,
           "a cover is for paired peers only");

    // A body signed but not sealed for the host, as a peer that never had the secret could not
    // send, is refused before it is read.
    const std::string plain = R"({"system":"gb","game":"Test"})";
    const std::string fresher = secrets::hexText(lan::crypto::randomBytes(lan::crypto::nonceBytes));
    lucent::http::Request unsealed{"POST", std::string{lan::wire::saveStatusRoute}, {}, plain};
    unsealed.headers = {
        {std::string{lan::peerHeader}, peer},
        {std::string{lan::timeHeader}, std::to_string(now)},
        {std::string{lan::nonceHeader}, fresher},
        {std::string{lan::signatureHeader},
         lan::requestSignature(
             secret, lan::RequestFacts{"POST", lan::wire::saveStatusRoute, now, fresher, plain})}};
    expect(listener.handle(unsealed).status == 403, "a body that does not open is refused");
}

void bothSidesDeriveOneSecret() {
    const lan::crypto::KeyPair guest;
    const lan::crypto::KeyPair host;
    const lan::PairingTranscript transcript{
        .guestId = "g",
        .hostId = "h",
        .guestKey = guest.publicKey(),
        .guestNonce = lan::crypto::randomBytes(lan::crypto::nonceBytes),
        .hostKey = host.publicKey(),
        .hostNonce = lan::crypto::randomBytes(lan::crypto::nonceBytes)};
    const lan::crypto::Bytes guestShared = must(guest.agree(host.publicKey()), "X25519 agrees");
    const lan::crypto::Bytes hostShared = must(host.agree(guest.publicKey()), "X25519 agrees");
    expect(guestShared == hostShared, "both ends agree on one secret");
    expect(lan::pairingSecret(guestShared, transcript) ==
               lan::pairingSecret(hostShared, transcript),
           "both keep the same secret");
    const std::string code = lan::pairingCode(transcript);
    expect(code.size() == 7 && code[3] == ' ' &&
               std::ranges::count_if(code,
                                     [](char c) {
                                         return c >= '0' && c <= '9';
                                     }) == 6,
           "a code is two groups of three digits");

    lan::HostPairing pairing{"h"};
    const lan::crypto::Bytes nonce = lan::crypto::randomBytes(lan::crypto::nonceBytes);
    const auto at = lan::HostPairing::Clock::now();
    std::string refusal;
    expect(pairing
               .begin(lan::Peer{"g", "Guest"}, lan::pairingCommitment(guest.publicKey(), nonce), at,
                      refusal)
               .has_value(),
           "a guest's commitment is answered");
    const lan::crypto::KeyPair swapped;
    expect(!pairing.begin(lan::Peer{"x", "Other"},
                          lan::pairingCommitment(swapped.publicKey(), nonce), at, refusal),
           "one pairing at a time");
    expect(!pairing.reveal("g", swapped.publicKey(), nonce, at, refusal),
           "a key other than the one committed to is refused");
    expect(!pairing.question(at), "and nothing is asked of the player");
    expect(!pairing.reveal("g", guest.publicKey(), nonce, at, refusal),
           "a refused reveal ends that pairing");
    expect(!pairing.begin(lan::Peer{"g", "Guest"}, lan::pairingCommitment(guest.publicKey(), nonce),
                          at, refusal),
           "a new offer waits out the spacing, so a man in the middle cannot grind codes");
    const auto later = at + lan::offerSpacing;
    expect(pairing.begin(lan::Peer{"g", "Guest"}, lan::pairingCommitment(guest.publicKey(), nonce),
                         later, refusal)
                   .has_value() &&
               pairing.reveal("g", guest.publicKey(), nonce, later, refusal) &&
               pairing.question(later).has_value(),
           "asked again after it and revealed as committed, the player is asked");
    pairing.decide("g", lan::Verdict::Declined, later);
    expect(pairing
               .begin(lan::Peer{"x", "Other"}, lan::pairingCommitment(swapped.publicKey(), nonce),
                      later, refusal)
               .has_value(),
           "once the player answered, the next guest may ask at once");
    expect(!pairing.question(later + lan::patience + std::chrono::seconds{1}),
           "a pairing past its time lapses");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "lan-node"};
    bothSidesDeriveOneSecret();
    signedRequestsAreCheckedOnce(scratch.path());
    pairedNodesListEachOthersGames(scratch.path());
    bundlesStayInTheirSlots();
    sharedSavesFollowThePlayer(scratch.path());
    std::printf("lan_node: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

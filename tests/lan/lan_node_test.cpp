// LAN pairing and remote libraries between openSUs in one process: real listeners on loopback,
// the discovery each node would get from Avahi stood in by a shared directory of announcements.
// Pairing shows the same code on both sides and leaves both books holding the same secret; each
// then lists the other's installed games and covers; an unpaired openSU learns only the name;
// signed requests that are forged, late or repeated are refused, and their bodies sealed;
// forgetting revokes both sides. Saves shared both ways follow the player: pulled before a game,
// pushed after, a conflict left for the player to pick, every replaced save backed up. ROMs a
// paired host lists are imported in sealed chunks, resumed from a part, checked by their hash and
// placed in the guest's ROM folder; a path outside the host's folders is refused.
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
#include <set>
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
#include "lan/remote_library.hpp"
#include "lan/request_signature.hpp"
#include "lan/rom_import.hpp"
#include "lan/rom_shelf.hpp"
#include "lan/shared_library.hpp"
#include "lan/wire.hpp"
#include "library/rom_tree.hpp"
#include "net/web_client.hpp"
#include "profiles/profile_book.hpp"
#include "save_sync/save_bundle.hpp"
#include "save_sync/save_sync.hpp"
#include "secrets/crypto.hpp"
#include "secrets/secret.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

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
    /// Whether `id` is heard on the LAN, as a machine that leaves and comes back.
    void hear(const std::string& id, bool heard) {
        const std::lock_guard lock{mutex_};
        if (heard) {
            unheard_.erase(id);
        } else {
            unheard_.insert(id);
        }
    }
    [[nodiscard]] std::vector<lan::Sighting> except(const std::string& id) const {
        const std::lock_guard lock{mutex_};
        std::vector<lan::Sighting> seen;
        for (const auto& [peer, sighting] : announced_) {
            if (peer != id && !unheard_.contains(peer)) {
                seen.push_back(sighting);
            }
        }
        return seen;
    }

  private:
    mutable std::mutex mutex_;
    std::map<std::string, lan::Sighting> announced_;
    std::set<std::string> unheard_;
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
          sync{save_sync::SaveSync::Setup{
              .versionsFile = state / "save-sync.json",
              .backups = state,
              .staging = state / "saves-incoming",
              .runnerSaves = saves,
              .inUse =
                  [this](const library::Game&, const library::saves::EmulatorSaves&) {
                      const std::lock_guard lock{mutex};
                      return busy;
                  },
              .changed =
                  [this] {
                      ++savesChanged;
                  },
              .seatOf =
                  [this](const std::string& profile) {
                      return profiles.seatOf(profile);
                  }}},
          profiles{root / name / "profiles.json"},
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
              .saves = sync,
              .profiles = profiles,
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
              .roms = lan::RomImport::Setup{.selfId = {},
                                            .staging = state / "lan-roms",
                                            .link = {},
                                            .place =
                                                [](const lan::RomImport::Item&,
                                                   const lan::RomImport::File& file,
                                                   const fs::path& staged, std::string& error) {
                                                    return library::roms::FileTree{}.place(
                                                        file.folder, file.place, staged, error);
                                                },
                                            .changed =
                                                [] {
                                                }},
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
    save_sync::SaveSync sync;
    profiles::ProfileBook profiles;
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
    host.node.share({installedGame("own:1", "Shared game", cover), uninstalled});
    guest.node.share({installedGame("own:9", "Guest game", {})});

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
    const secrets::crypto::Bytes guessed = secrets::crypto::randomBytes(secrets::crypto::keyBytes);
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
        return save_sync::BundleFile{
            .kind = SlotKind::Memory, .path = std::move(path), .bytes = "x", .modified = {}};
    };
    const save_sync::SaveBundle saves{{file("slot/one.sav"), file("two.sav")}};
    const auto read = save_sync::readBundle(save_sync::write(saves));
    expect(read && read->hash() == saves.hash() && read->bytes() == 2,
           "a bundle reads back as written");
    for (const char* outside : {"../x", "a/../../x", "/etc/x", "a\\x", "c:x", "a//x", "./x"}) {
        expect(!save_sync::readBundle(save_sync::write(save_sync::SaveBundle{{file(outside)}})),
               "a path outside its slot is refused");
    }
    expect(!save_sync::readBundle(save_sync::write(save_sync::SaveBundle{{file("a"), file("a")}})),
           "and so are two files at one path");
    expect(!save_sync::readBundle("99\n{\"files\":[]}"), "and a header longer than the bundle");
}

/// The standing of `node`'s saves of `game` with its one sharing peer, once a look settles.
std::optional<save_sync::TargetSaves::Standing> standingOf(Node& node, const library::Game& game,
                                                           const std::string& player) {
    const std::vector<save_sync::TargetSaves> standings = node.sync.standings(game.id, player);
    if (standings.size() != 1) {
        return std::nullopt;
    }
    return standings.front().standing;
}

/// Looks at `game` on `node` until its saves stand as `expected` with the peer.
bool looksLike(Node& node, const library::Game& game, const std::string& player,
               save_sync::TargetSaves::Standing expected) {
    return waitFor([&node, &game, &player, expected] {
        if (node.sync.busy(game.id)) {
            return false;
        }
        node.sync.look(game, player);
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        return standingOf(node, game, player) == expected;
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
    using Standing = save_sync::TargetSaves::Standing;
    Directory directory;
    Node host{root, "Den", directory};
    Node guest{root, "Attic", directory};
    const library::Game game = runnerGame();
    host.node.share({game});
    guest.node.share({game});
    host.sync.know({game}, runnerCatalog());
    guest.sync.know({game}, runnerCatalog());
    // One player's profile on both: the den's, which the attic copies.
    const std::string player = host.profiles.first().id;
    std::string error;
    const fs::path hostSave = host.saves / "profiles" / player / "gb" / "Test.srm";
    const fs::path guestSave = guest.saves / "profiles" / player / "gb" / "Test.srm";
    expect(waitFor([&guest] {
               return guest.node.sightings().size() == 1;
           }),
           "the two openSUs find each other");
    expect(pairUp(guest, host, true) == GuestPairing::Phase::Paired, "they pair");

    writeFile(hostSave, "saved on the den");
    const std::string own = guest.profiles.first().id;
    const std::uint64_t unshared = guest.sync.prepare(game, own);
    expect(waitFor([&guest, unshared] {
               return guest.sync.prepared(unshared);
           }),
           "a launch as the attic's own profile is let go at once: it is linked with nothing");
    expect(!fs::exists(guestSave) && looksLike(guest, game, own, Standing::NotShared),
           "and nothing is taken from the den");

    const std::optional<std::vector<lan::wire::ListedProfile>> listed =
        guest.node.profilesOf(host.node.id(), error);
    expect(listed && listed->size() == 1 && listed->front().id == player &&
               listed->front().name == "Player 1" && !listed->front().linked,
           "the attic lists the den's profile, not linked");
    expect(guest.node.askCopy(host.node.id(), player, error), "the attic asks for a copy of it");
    const std::optional<lan::ProfileCopies::Question> asked = host.node.copyQuestion();
    expect(asked && asked->peer.id == guest.node.id() && asked->profile.id == player,
           "the den's player is asked");
    const std::optional<lan::wire::CopyVerdict> waiting =
        guest.node.copyStatus(host.node.id(), player, error);
    expect(waiting && waiting->verdict == "waiting", "the attic waits for the answer");
    expect(host.node.answerCopy(guest.node.id(), player, true, error) &&
               host.profiles.linked(player, guest.node.id()),
           "the den's player allows it, and the den links its profile with the attic");
    const lan::wire::CopyVerdict allowed =
        test::need(guest.node.copyStatus(host.node.id(), player, error), "a verdict");
    expect(allowed.verdict == "accepted" && allowed.profile.id == player &&
               allowed.profile.name == "Player 1",
           "the attic hears it is allowed, with the profile");
    expect(guest.profiles.adopt(profiles::Profile{.id = allowed.profile.id,
                                                  .name = allowed.profile.name,
                                                  .picture = allowed.profile.picture,
                                                  .links = {}},
                                host.node.id(), error) &&
               guest.profiles.linked(player, host.node.id()),
           "and keeps the copy, linked with the den");
    expect(guest.sync.sharing(game), "so the game's saves are shared");
    expect(looksLike(guest, game, player, Standing::NewerThere), "the den's save is newer");
    const std::uint64_t ticket = guest.sync.prepare(game, player);
    expect(waitFor([&guest, ticket] {
               return guest.sync.prepared(ticket);
           }),
           "before the game starts here, its saves are synced");
    expect(readFile(guestSave) == "saved on the den",
           "a save made on the den is here before the game starts");
    expect(standingOf(guest, game, player) == Standing::InSync, "and both are in sync");

    writeFile(guestSave, "played in the attic");
    guest.sync.sync(game, player);
    expect(waitFor([&hostSave] {
               return readFile(hostSave) == "played in the attic";
           }),
           "a change here goes to the den after the game ends");
    expect(holdsCopy(host.state, "saved on the den"), "the den's save it replaced is backed up");
    expect(looksLike(guest, game, player, Standing::InSync) &&
               looksLike(host, game, player, Standing::InSync),
           "and both say they are in sync");

    writeFile(hostSave, "changed on the den");
    writeFile(guestSave, "changed in the attic");
    expect(looksLike(guest, game, player, Standing::Conflict) &&
               looksLike(host, game, player, Standing::Conflict),
           "a save changed on both sides is a conflict on both");
    guest.sync.sync(game, player);
    expect(looksLike(guest, game, player, Standing::Conflict) &&
               readFile(hostSave) == "changed on the den" &&
               readFile(guestSave) == "changed in the attic",
           "which a sync leaves alone");

    host.setBusy("Close mGBA first: it is running");
    guest.sync.keep(game, player, host.node.id(), false);
    expect(waitFor([&host, &game, &player] {
               const auto standings = host.sync.standings(game.id, player);
               return !host.sync.busy(game.id) && standings.size() == 1 &&
                      standings.front().standing == Standing::Failed &&
                      standings.front().problem.find("running") != std::string::npos;
           }),
           "keeping the attic's is refused on the den while its emulator runs");
    expect(readFile(hostSave) == "changed on the den", "and the den's save is untouched");

    host.setBusy({});
    host.sync.keep(game, player, guest.node.id(), true);
    expect(waitFor([&hostSave] {
               return readFile(hostSave) == "changed in the attic";
           }),
           "once it closed, the player there keeps the attic's");
    expect(holdsCopy(host.state, "changed on the den"), "the save it replaced is backed up");
    expect(looksLike(host, game, player, Standing::InSync) &&
               looksLike(guest, game, player, Standing::InSync),
           "and the two are in sync again");

    const std::string other =
        test::need(host.profiles.add("Ana", error), "a second profile on the den").id;
    writeFile(host.saves / "profiles" / other / "gb" / "Test.srm", "Ana's on the den");
    expect(looksLike(host, game, other, Standing::NotShared),
           "a profile of the den's not linked with the attic does not sync with it");

    expect(guest.node.unlink(host.node.id(), player, error) &&
               !guest.profiles.linked(player, host.node.id()),
           "the attic unlinks the copy");
    expect(!host.profiles.linked(player, guest.node.id()) && guest.profiles.find(player) &&
               host.profiles.find(player),
           "the den is told and unlinks it too; both keep the profile");
    expect(looksLike(host, game, player, Standing::NotShared), "and its saves no longer sync");
}

/// The signed routes, asked directly of a listener.

/// `bytes` bytes that differ from one run to the next.
std::string noise(std::size_t bytes) {
    const secrets::crypto::Bytes random = secrets::crypto::randomBytes(bytes);
    return {random.begin(), random.end()};
}

library::Game romGame(const fs::path& file, const std::string& system, const std::string& title) {
    library::Game game;
    game.id = "rom:" + file.string();
    game.source = library::Source::Rom;
    game.sourceId = system;
    game.title = title;
    game.file = file;
    game.installed = true;
    return game;
}

/// The import of `id`, once it ended (done or failed).
lan::RomImport::Progress importEnded(Node& guest, const std::string& id) {
    std::optional<lan::RomImport::Progress> ended;
    waitFor([&] {
        for (const lan::RomImport::Progress& progress : guest.node.imports().progress()) {
            if (progress.id == id && (progress.state == lan::RomImport::State::Done ||
                                      progress.state == lan::RomImport::State::Failed)) {
                ended = progress;
                return true;
            }
        }
        return false;
    });
    return must(ended, "the import ends");
}

/// The game `title` of the listing as an import into `folder`.
lan::RomImport::Item itemOf(const std::vector<lan::wire::RomConsole>& consoles, Node& host,
                            const std::string& title, const fs::path& folder) {
    for (const lan::wire::RomConsole& console : consoles) {
        for (const lan::wire::RomGame& game : console.games) {
            if (game.title != title) {
                continue;
            }
            lan::RomImport::Item item{"rom:" + game.entry, host.node.id(), "Host",
                                      game.title,          console.system, {}};
            for (const lan::wire::RomFile& file : game.files) {
                item.files.push_back(lan::RomImport::File{
                    lan::wire::RomKind::Rom, game.root, file.path, file.bytes, folder, file.path});
            }
            return item;
        }
    }
    fail("the listing holds the game");
}

void romsComeOverInChunks(const fs::path& root) {
    Directory directory;
    Node host{root, "Rom host", directory};
    Node guest{root, "Rom guest", directory};
    expect(waitFor([&guest] {
               return guest.node.sightings().size() == 1;
           }),
           "the guest finds the host");
    expect(pairUp(guest, host, true) == GuestPairing::Phase::Paired, "the two pair");

    const fs::path roms = root / "host-roms";
    // Larger than two chunks, so it crosses in three.
    const std::string big = noise(static_cast<std::size_t>(lan::wire::romChunkBytes * 2 + 4321));
    writeFile(roms / "Game Boy" / "Big.gb", big);
    writeFile(roms / "PSX" / "Disc" / "Disc.cue", "FILE \"Disc.bin\" BINARY\n");
    writeFile(roms / "PSX" / "Disc" / "Disc.bin", noise(70000));
    writeFile(roms / "Game Boy" / "Unlisted.gb", "not in the catalog");
    writeFile(root / "outside.txt", "private");
    const fs::path bios = root / "host-bios";
    writeFile(bios / "scph5501.bin", noise(512));
    host.node.shareRoms(lan::RomShelf::Contents{
        .roots = {roms},
        .games = {romGame(roms / "Game Boy" / "Big.gb", "gb", "Big"),
                  romGame(roms / "PSX" / "Disc" / "Disc.cue", "psx", "Disc")},
        .biosFolder = bios,
        .bios = {{"psx", {"scph5501.bin", "missing.bin"}}}});

    std::string error;
    const std::vector<lan::wire::RomConsole> consoles =
        must(guest.node.romsOf(host.node.id(), error), "the guest lists the host's ROMs");
    expect(consoles.size() == 2, "a console per folder of games");
    const auto psx = std::ranges::find(consoles, "psx", &lan::wire::RomConsole::system);
    expect(psx != consoles.end() && psx->games.size() == 1 && psx->games[0].files.size() == 2 &&
               psx->games[0].entry == "PSX/Disc",
           "a game folder is listed with every file in it");
    expect(psx->bios.size() == 1 && psx->bios[0].path == "scph5501.bin" &&
               psx->bios[0].bytes == 512,
           "the BIOS files the host has for the console are listed, and no others");

    const fs::path mine = root / "guest-roms";
    const lan::RomImport::Item disc = itemOf(consoles, host, "Disc", mine);
    lan::RomImport::Item bigItem = itemOf(consoles, host, "Big", mine);
    // A part already holding the start of the file, as a dropped connection leaves it.
    const fs::path part =
        guest.state / "lan-roms" / host.node.id() / "rom-0" / "Game Boy" / "Big.gb.part";
    writeFile(part, big.substr(0, static_cast<std::size_t>(lan::wire::romChunkBytes) + 99));
    guest.node.imports().add({disc, bigItem});
    expect(importEnded(guest, disc.id).state == lan::RomImport::State::Done,
           "the folder game arrives");
    const lan::RomImport::Progress bigDone = importEnded(guest, bigItem.id);
    expect(bigDone.state == lan::RomImport::State::Done && bigDone.done == big.size(),
           "the large ROM arrives, all of it");
    expect(readFile(mine / "Game Boy" / "Big.gb") == big,
           "it is the host's bytes, resumed from the part");
    expect(readFile(mine / "PSX" / "Disc" / "Disc.cue") ==
                   readFile(roms / "PSX" / "Disc" / "Disc.cue") &&
               readFile(mine / "PSX" / "Disc" / "Disc.bin") ==
                   readFile(roms / "PSX" / "Disc" / "Disc.bin"),
           "every file of the game folder is in the guest's folder");
    expect(!fs::exists(part), "the part is gone once the file is in place");

    // A part that does not hold the host's bytes fails the hash and is thrown away.
    writeFile(part, std::string(1000, 'x'));
    fs::remove(mine / "Game Boy" / "Big.gb");
    guest.node.imports().add({bigItem});
    const lan::RomImport::Progress damaged = importEnded(guest, bigItem.id);
    expect(damaged.state == lan::RomImport::State::Failed &&
               damaged.note.find("arrived different") != std::string::npos,
           "a file whose hash is not the host's is refused");
    expect(!fs::exists(part) && !fs::exists(mine / "Game Boy" / "Big.gb"), "nothing of it is kept");

    // The host leaves the LAN: the import waits, and goes on when it is back.
    directory.hear(host.node.id(), false);
    guest.node.imports().add({bigItem});
    expect(waitFor([&] {
               const auto all = guest.node.imports().progress();
               const auto it = std::ranges::find(all, bigItem.id, &lan::RomImport::Progress::id);
               return it != all.end() && it->state == lan::RomImport::State::Waiting;
           }),
           "an import waits while its host is away");
    directory.hear(host.node.id(), true);
    expect(importEnded(guest, bigItem.id).state == lan::RomImport::State::Done &&
               readFile(mine / "Game Boy" / "Big.gb") == big,
           "and arrives once the host is back");
}

void signedRequestsAreCheckedOnce(const fs::path& root) {
    lan::PeerBook book{root / "signed" / "lan.json",
                       secrets::SecretStore{root / "signed" / "secrets", false}};
    lan::HostPairing pairing{book.id()};
    lan::SharedLibrary library;
    library.share({installedGame("own:1", "Shared game", {})});
    lan::RomShelf shelf;
    const net::WebClient web;
    profiles::ProfileBook profiles{root / "signed" / "profiles.json"};
    lan::ProfileCopies copies{profiles};
    save_sync::SaveSync saves{save_sync::SaveSync::Setup{
        .versionsFile = root / "signed" / "save-sync.json",
        .backups = root / "signed",
        .staging = root / "signed" / "incoming",
        .runnerSaves = root / "signed" / "saves",
        .inUse =
            [](const library::Game&, const library::saves::EmulatorSaves&) {
                return std::string{};
            },
        .changed =
            [] {
            },
        .seatOf =
            [&profiles](const std::string& profile) {
                return profiles.seatOf(profile);
            }}};
    lan::LanListener listener{lan::LanListener::Sources{.book = book,
                                                        .pairing = pairing,
                                                        .library = library,
                                                        .saves = saves,
                                                        .profiles = profiles,
                                                        .copies = copies,
                                                        .roms = shelf,
                                                        .name =
                                                            [] {
                                                                return std::string{"Host"};
                                                            },
                                                        .unpaired =
                                                            [](const lan::Peer&) {
                                                            },
                                                        .play = {}},
                              0};
    const secrets::crypto::Bytes secret = secrets::crypto::randomBytes(secrets::crypto::keyBytes);
    const std::string peer = secrets::hexText(secrets::crypto::randomBytes(lan::peerIdBytes));
    std::string error;
    expect(book.remember(lan::Peer{peer, "Guest"}, secret, error), "the guest is paired");

    const auto request = [&peer](const secrets::crypto::Bytes& key, std::int64_t time,
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
    const std::string nonce =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));

    const lucent::http::Response answer = listener.handle(request(secret, now, nonce));
    expect(answer.status == 200, "a signed request is answered");
    expect(answer.body.find("Shared game") == std::string::npos,
           "the answer does not cross the LAN in clear");
    const lan::RequestFacts facts{"GET", "/lan/library", now, nonce, {}};
    const secrets::crypto::Bytes other = secrets::crypto::randomBytes(secrets::crypto::keyBytes);
    expect(!lan::openBody(other, facts, lan::Direction::Answer, 200, answer.body),
           "it opens with no other secret");
    const std::string otherNonce =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
    const lan::RequestFacts otherRequest{"GET", "/lan/library", now, otherNonce, {}};
    expect(!lan::openBody(secret, otherRequest, lan::Direction::Answer, 200, answer.body),
           "nor as the answer to another request");
    const auto opened = lan::openBody(secret, facts, lan::Direction::Answer, 200, answer.body);
    const auto listing = opened ? lan::wire::readListing(*opened) : std::nullopt;
    expect(listing && listing->size() == 1 && listing->front().title == "Shared game",
           "sealed with the pairing's secret for this request, it holds the shared listing");

    expect(listener.handle(request(secret, now, nonce)).status == 403, "a repeat is refused");
    const std::string fresh =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
    expect(listener.handle(request(secret, now - 3600, fresh)).status == 403,
           "a request an hour late is refused");
    const std::string another =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
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
    const std::string fresher =
        secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
    lucent::http::Request unsealed{"POST", std::string{lan::wire::saveStatusRoute}, {}, plain};
    unsealed.headers = {
        {std::string{lan::peerHeader}, peer},
        {std::string{lan::timeHeader}, std::to_string(now)},
        {std::string{lan::nonceHeader}, fresher},
        {std::string{lan::signatureHeader},
         lan::requestSignature(
             secret, lan::RequestFacts{"POST", lan::wire::saveStatusRoute, now, fresher, plain})}};
    expect(listener.handle(unsealed).status == 403, "a body that does not open is refused");

    // ROM routes read only the files the shelf lists.
    const fs::path roms = root / "signed" / "roms";
    writeFile(roms / "NES" / "Listed.nes", "listed bytes");
    writeFile(roms / "NES" / "Unlisted.nes", "unlisted bytes");
    writeFile(root / "signed" / "secret.txt", "private");
    shelf.share(
        lan::RomShelf::Contents{.roots = {roms},
                                .games = {romGame(roms / "NES" / "Listed.nes", "nes", "Listed")},
                                .biosFolder = {},
                                .bios = {}});
    const auto chunk = [&](const std::string& path) {
        const std::string route{lan::wire::romChunkRoute};
        const std::string body =
            lan::wire::write(lan::wire::RomAsk{lan::wire::RomKind::Rom, 0, path, 0, 0});
        const std::string asked =
            secrets::hexText(secrets::crypto::randomBytes(secrets::crypto::nonceBytes));
        const lan::RequestFacts sent{"POST", route, now, asked, {}};
        const std::string sealed = lan::sealBody(secret, sent, lan::Direction::Request, 0, body);
        lucent::http::Request post{"POST", route, {}, sealed};
        post.headers = {
            {std::string{lan::peerHeader}, peer},
            {std::string{lan::timeHeader}, std::to_string(now)},
            {std::string{lan::nonceHeader}, asked},
            {std::string{lan::signatureHeader},
             lan::requestSignature(secret, lan::RequestFacts{"POST", route, now, asked, sealed})}};
        const lucent::http::Response answered = listener.handle(post);
        return std::pair{answered.status, lan::openBody(secret, sent, lan::Direction::Answer,
                                                        answered.status, answered.body)
                                              .value_or("")};
    };
    const auto listed = chunk("NES/Listed.nes");
    expect(listed.first == 200 && listed.second == "listed bytes", "a listed ROM is sent");
    expect(listed.second.size() < lan::wire::romChunkBytes, "in one chunk when it fits");
    expect(chunk("NES/Unlisted.nes").first == 403, "a file the catalog does not list is refused");
    expect(chunk("../secret.txt").first == 403, "a path out of the ROM folder is refused");
    expect(chunk("NES/../../secret.txt").first == 403, "however it is spelled");
}

void bothSidesDeriveOneSecret() {
    const secrets::crypto::KeyPair guest;
    const secrets::crypto::KeyPair host;
    const lan::PairingTranscript transcript{
        .guestId = "g",
        .hostId = "h",
        .guestKey = guest.publicKey(),
        .guestNonce = secrets::crypto::randomBytes(secrets::crypto::nonceBytes),
        .hostKey = host.publicKey(),
        .hostNonce = secrets::crypto::randomBytes(secrets::crypto::nonceBytes)};
    const secrets::crypto::Bytes guestShared = must(guest.agree(host.publicKey()), "X25519 agrees");
    const secrets::crypto::Bytes hostShared = must(host.agree(guest.publicKey()), "X25519 agrees");
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
    const secrets::crypto::Bytes nonce = secrets::crypto::randomBytes(secrets::crypto::nonceBytes);
    const auto at = lan::HostPairing::Clock::now();
    std::string refusal;
    expect(pairing
               .begin(lan::Peer{"g", "Guest"}, lan::pairingCommitment(guest.publicKey(), nonce), at,
                      refusal)
               .has_value(),
           "a guest's commitment is answered");
    const secrets::crypto::KeyPair swapped;
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
    romsComeOverInChunks(scratch.path());
    std::printf("lan_node: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

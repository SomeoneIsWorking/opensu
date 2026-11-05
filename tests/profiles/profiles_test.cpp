// The profile book kept across runs, and the saves from before profiles moved into the first
// profile's once, backed up first, going on after a move cut short.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "profiles/profile_book.hpp"
#include "profiles/save_move.hpp"
#include "save_sync/save_versions.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void writeFile(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << bytes;
}

std::string readFile(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void theBookKeepsItsProfiles(const fs::path& root) {
    const fs::path file = root / "profiles.json";
    std::string first;
    std::string added;
    {
        profiles::ProfileBook book{file};
        expect(book.all().size() == 1 && book.first().name == profiles::firstProfileName,
               "a new book has its first profile");
        first = book.first().id;
        expect(profiles::isProfileId(first) && book.active().id == first,
               "with an id, and it is the active one");
        std::string error;
        const profiles::Profile ana = test::need(book.add("Ana", error), "a second profile");
        expect(ana.picture == 1, "a second profile takes the next picture");
        added = ana.id;
        expect(book.activate(added, error) && book.active().id == added, "it can be made active");
        expect(book.seatOf(added) == 1, "it sits second");
        expect(book.link(added, "peer", true, error) && book.linked(added, "peer"),
               "it links with a peer");
        expect(!book.linked(first, "peer"), "the other profile does not");
    }
    profiles::ProfileBook again{file};
    expect(again.first().id == first && again.active().id == added &&
               again.linked(added, "peer").has_value(),
           "the book reads back as kept");
    std::string error;
    const profiles::Profile copy{
        .id = std::string(32, 'a'), .name = "Bo", .picture = 3, .links = {}};
    expect(again.adopt(copy, "desk", error) && again.linked(copy.id, "desk"),
           "a copy from another openSU is adopted, linked with it");
    expect(again.forgetPeer("desk", error) && !again.linked(copy.id, "desk") &&
               again.find(copy.id).has_value(),
           "forgetting the peer keeps the profile, unlinked");
}

void savesMoveIntoTheFirstProfileOnce(const fs::path& root) {
    const fs::path saves = root / "saves";
    const fs::path state = root / "state";
    const profiles::SaveMove move{
        .saves = saves, .state = state, .versions = state / save_sync::saveVersionsFile};
    writeFile(saves / "gb" / "Test.srm", "game boy");
    writeFile(saves / "gba" / "Kirby.srm", "kirby");
    writeFile(saves / "gba" / "Kirby.state", "state");
    writeFile(state / "save-backups" / "gba" / "Kirby" / "built-in" / "20260101T000000Z" /
                  "backup.json",
              "{}");
    writeFile(move.versions, R"({"games":[{"system":"gba","game":"Kirby","counter":4,"hash":"h",)"
                             R"("synced":{"share:x":"h"}}]})");
    const std::string id(32, 'b');
    const fs::path into = saves / "profiles" / id;

    // A move cut short: the backup half made, one file already in place.
    writeFile(state / "save-backups" / "before-profiles" / "20260101T000000Z.part" / "gb" /
                  "Test.srm",
              "half");
    writeFile(into / "gb" / "Test.srm", "game boy");
    fs::remove(saves / "gb" / "Test.srm");

    const auto now = std::chrono::system_clock::now();
    const profiles::Moved moved = profiles::moveIntoProfile(move, id, now);
    expect(moved.files == 3 && moved.left == 0, "every file left over moves");
    expect(readFile(into / "gba" / "Kirby.srm") == "kirby" &&
               readFile(into / "gba" / "Kirby.state") == "state" &&
               readFile(into / "gb" / "Test.srm") == "game boy",
           "into the profile's folder, by the same paths");
    expect(!fs::exists(saves / "gba") && !fs::exists(saves / "gb"), "and the old folders are gone");
    expect(readFile(moved.backup / "gba" / "Kirby.srm") == "kirby", "the saves were backed up");
    expect(!fs::exists(moved.backup.string() + ".part") &&
               !fs::exists(state / "save-backups" / "before-profiles" / "20260101T000000Z.part"),
           "whole, with no part left");
    expect(fs::exists(state / "save-backups" / "profiles" / id / "gba" / "Kirby" / "built-in" /
                      "20260101T000000Z" / "backup.json"),
           "Copy saves' backups move into the profile's");
    const save_sync::SaveVersions versions{move.versions};
    const library::saves::SaveKey kirby{.system = "gba", .game = "Kirby", .profile = id};
    expect(versions.syncedWith(kirby, "share:x") == "h", "the sync record is the profile's");

    writeFile(saves / "gba" / "Kirby.srm", "an older openSU wrote this");
    const profiles::Moved twice = profiles::moveIntoProfile(move, id, now);
    expect(twice.files == 0 && twice.left == 1 && twice.backup.empty() &&
               readFile(into / "gba" / "Kirby.srm") == "kirby",
           "a file whose place is taken stays where it is, with no new backup");
    fs::remove_all(saves / "gba");
    const profiles::Moved done = profiles::moveIntoProfile(move, id, now);
    expect(done.files == 0 && done.left == 0 && done.backup.empty(),
           "once moved, the move does nothing");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "profiles"};
    theBookKeepsItsProfiles(scratch.path() / "book");
    savesMoveIntoTheFirstProfileOnce(scratch.path() / "move");
    std::printf("profiles: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

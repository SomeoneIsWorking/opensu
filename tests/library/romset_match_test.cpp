// Arcade romsets: a listing read from a datafile and a -listxml, kept and read back, matched
// against zips on disk (parent, BIOS choice, device, disk, renamed members), the verdicts kept by
// file stamps, and the game's emulator picked by them. The listings are fetched off the scan from
// a loopback host: in turn, with their bytes, failures retried after their time, and a scan while
// one downloads gives a pending verdict without waiting.
#include "library/romset_match.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "library/emulator_choice.hpp"
#include "library/romset_dat.hpp"
#include "library/romset_dats.hpp"
#include "library/romset_lists.hpp"
#include "support/loopback_server.hpp"
#include "support/release_server.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "support/zip_fixture.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::applyEmulatorPicks;
using opensu::library::emulatorKey;
using opensu::library::EmulatorOption;
using opensu::library::EmulatorSource;
using opensu::library::Game;
using opensu::library::LaunchSpec;
using opensu::library::RomsetFit;
using opensu::library::Source;
using opensu::library::roms::applyRomsets;
using opensu::library::roms::checkingRomset;
using opensu::library::roms::EmulatorEntry;
using opensu::library::roms::FolderZips;
using opensu::library::roms::matchSet;
using opensu::library::roms::noRomsetMatch;
using opensu::library::roms::readZipMembers;
using opensu::library::roms::ReleaseSource;
using opensu::library::roms::RomsetDat;
using opensu::library::roms::RomsetDats;
using opensu::library::roms::RomsetFetch;
using opensu::library::roms::RomsetLists;
using opensu::library::roms::romsetListsDownloading;
using opensu::library::roms::RomsetMatcher;
using opensu::library::roms::romsetRetryAfter;
using opensu::library::roms::RomsetSource;
using opensu::library::roms::RomsetVerdict;
using opensu::settings::Layer;
using opensu::settings::SettingLayers;
using opensu::test::LoopbackServer;
using opensu::test::ReleaseServer;
using opensu::test::ScratchDir;
using opensu::test::zipfixture::buildZip;
using opensu::test::zipfixture::crcOf;
using opensu::test::zipfixture::FixtureEntry;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

// What the ROMs hold; their CRC-32s are what the listings name.
constexpr std::string_view program = "program rom";
constexpr std::string_view graphics = "graphics rom";
constexpr std::string_view cloneProgram = "clone program";
constexpr std::string_view biosEurope = "bios europe";
constexpr std::string_view biosJapan = "bios japan";
constexpr std::string_view dsp = "dsp program";

std::string hex(std::string_view data) {
    return std::format("{:08x}", crcOf(std::string{data}));
}

/// A Logiqx datafile as FinalBurn Neo ships: parent, clone with merged ROMs, a nodump.
std::string datafile() {
    return std::format(R"(<?xml version="1.0"?>
<!DOCTYPE datafile PUBLIC "-//FinalBurn Neo//DTD ROM Management Datafile//EN" "x">
<datafile>
	<header><name>FinalBurn Neo</name></header>
	<game name="Parent">
		<description>Parent &amp; Co</description>
		<rom name="p.1" size="{}" crc="{}"/>
		<rom name="g.1" size="{}" crc="{}"/>
		<rom name="missing.1" size="16" crc="00000000" status="nodump"/>
	</game>
	<game name="clone" cloneof="parent" romof="parent">
		<rom name="c.1" size="{}" crc="{}"/>
		<rom name="g.1" merge="g.1" size="{}" crc="{}"/>
	</game>
</datafile>
)",
                       program.size(), hex(program), graphics.size(), hex(graphics),
                       cloneProgram.size(), hex(cloneProgram), graphics.size(), hex(graphics));
}

/// A -listxml as current MAME writes it: a BIOS with two choices, a device with a ROM, a disk.
std::string listxml() {
    return std::format(R"(<?xml version="1.0"?>
<!DOCTYPE mame [
<!ELEMENT mame (machine+)>
	<!ATTLIST machine name CDATA #REQUIRED>
]>
<mame build="0.289">
	<machine name="bios" isbios="yes">
		<biosset name="euro" description="Europe" default="yes"/>
		<biosset name="japan" description="Japan"/>
		<rom name="eu.bin" bios="euro" size="{}" crc="{}"/>
		<rom name="jp.bin" bios="japan" size="{}" crc="{}"/>
	</machine>
	<machine name="game" romof="bios">
		<rom name="eu.bin" merge="eu.bin" bios="euro" size="{}" crc="{}"/>
		<rom name="jp.bin" merge="jp.bin" bios="japan" size="{}" crc="{}"/>
		<rom name="p.1" size="{}" crc="{}"/>
		<rom name="opt.1" size="4" crc="12345678" optional="yes"/>
		<device_ref name="m68000"/>
		<device_ref name="qsound_hle"/>
	</machine>
	<machine name="disky">
		<rom name="p.1" size="{}" crc="{}"/>
		<disk name="disky-hdd" sha1="00"/>
	</machine>
	<machine name="qsound_hle" isdevice="yes" runnable="no">
		<device_ref name="qsound"/>
	</machine>
	<machine name="qsound" isdevice="yes" runnable="no">
		<rom name="dl-1425.bin" size="{}" crc="{}"/>
	</machine>
	<machine name="m68000" isdevice="yes" runnable="no"/>
</mame>
)",
                       biosEurope.size(), hex(biosEurope), biosJapan.size(), hex(biosJapan),
                       biosEurope.size(), hex(biosEurope), biosJapan.size(), hex(biosJapan),
                       program.size(), hex(program), program.size(), hex(program), dsp.size(),
                       hex(dsp));
}

FixtureEntry member(std::string name, std::string_view data, bool deflate = false) {
    return FixtureEntry{std::move(name), std::string{data}, deflate, std::nullopt};
}

void writeZip(const fs::path& file, const std::vector<FixtureEntry>& entries) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary | std::ios::trunc} << buildZip(entries);
}

void parsesListings() {
    RomsetDat dat;
    dat.addXml(datafile());
    dat.addXml(listxml());
    expect(dat.size() == 8, "every set of both listings is kept, device and empty ones too");
    const auto* parent = dat.find("parent");
    expect(parent != nullptr && parent->roms.size() == 2,
           "a set is found by its lower-case name and a nodump ROM is left out");
    const auto* clone = dat.find("clone");
    expect(clone != nullptr && clone->romof == "parent" && clone->roms.size() == 2,
           "a clone keeps its parent and its merged ROMs");
    const auto* game = dat.find("game");
    expect(game != nullptr && game->roms.size() == 3 && game->roms[0].bios == "euro" &&
               game->devices == std::vector<std::string>{"m68000", "qsound_hle"},
           "an optional ROM is left out; BIOS choices and devices are kept");
    expect(dat.find("m68000") == nullptr && dat.link("m68000") != nullptr,
           "a device without ROMs or devices keeps only its link");
    expect(dat.link("bios") != nullptr && dat.link("bios")->defaultBios == "euro",
           "a BIOS's default choice is kept");
    expect(dat.find("disky") != nullptr && dat.find("disky")->disks.size() == 1,
           "a set's disk is kept");

    std::string error;
    const std::optional<RomsetDat> back = RomsetDat::parse(dat.serialize(), {}, error);
    expect(back && *back == dat, "the line format reads back as the same listing");
    const std::optional<RomsetDat> wanted =
        RomsetDat::parse(dat.serialize(), RomsetDat::Wanted{"clone"}, error);
    expect(wanted && wanted->find("clone") != nullptr && wanted->find("parent") == nullptr &&
               wanted->link("parent") != nullptr && wanted->find("qsound") != nullptr,
           "a wanted read keeps the named sets and devices with ROMs, and every link");
    expect(!RomsetDat::parse("S\tbroken\n", {}, error) && !error.empty(),
           "a malformed line is refused");
}

void matchesZips(const fs::path& base) {
    RomsetDat dat;
    dat.addXml(datafile());
    dat.addXml(listxml());
    const fs::path folder = base / "match";
    writeZip(folder / "clone.zip", {member("c.1", cloneProgram)});
    {
        FolderZips zips{folder};
        const RomsetVerdict alone = matchSet("clone", dat, zips);
        expect(alone.listed && alone.fit == RomsetFit::Mismatch && alone.required == 2 &&
                   alone.missing == std::vector<std::string>{"g.1"},
               "a clone without its parent lacks the merged ROM");
    }
    writeZip(folder / "parent.zip",
             {member("renamed.bin", program, true), member("g.1", graphics)});
    {
        FolderZips zips{folder};
        expect(matchSet("clone", dat, zips).fit == RomsetFit::Matches,
               "the parent's zip in the folder lends the clone its ROMs");
        expect(matchSet("parent", dat, zips).fit == RomsetFit::Matches,
               "a member is found by its CRC-32 and size whatever its name");
        const RomsetVerdict unknown = matchSet("nothing", dat, zips);
        expect(!unknown.listed && unknown.fit == RomsetFit::Mismatch,
               "a set the listing does not have is no match");
    }

    writeZip(folder / "game.zip", {member("p.1", program)});
    writeZip(folder / "bios.zip", {member("eu.bin", biosEurope)});
    {
        FolderZips zips{folder};
        const RomsetVerdict noDevice = matchSet("game", dat, zips);
        expect(noDevice.missing == std::vector<std::string>{"dl-1425.bin"} &&
                   noDevice.required == 3,
               "only the default BIOS choice is needed, and a device's device's ROM is");
    }
    writeZip(folder / "qsound.zip", {member("dl-1425.bin", dsp)});
    {
        FolderZips zips{folder};
        expect(matchSet("game", dat, zips).fit == RomsetFit::Matches,
               "the device's zip in the folder holds its ROM");
        expect(std::ranges::any_of(zips.looked(),
                                   [&](const auto& stamp) {
                                       return stamp.path == (folder / "qsound.zip").string() &&
                                              stamp.size > 0;
                                   }),
               "every zip the match read is recorded with its stamp");
    }

    writeZip(folder / "disky.zip", {member("p.1", program)});
    {
        FolderZips zips{folder};
        expect(matchSet("disky", dat, zips).missing == std::vector<std::string>{"disky-hdd.chd"},
               "a set's disk must be beside it");
    }
    fs::create_directories(folder / "disky");
    std::ofstream{folder / "disky" / "disky-hdd.chd"} << "chd";
    {
        FolderZips zips{folder};
        expect(matchSet("disky", dat, zips).fit == RomsetFit::Matches,
               "a disk in the set's folder is found");
    }

    std::string error;
    const auto members = readZipMembers(folder / "parent.zip", error);
    expect(members && members->size() == 2 && (*members)[0].name == "renamed.bin" &&
               (*members)[0].crc32 == crcOf(std::string{program}) &&
               (*members)[0].size == program.size(),
           "a zip's members are read from its central directory");
    std::ofstream{folder / "broken.zip"} << "not a zip";
    expect(!readZipMembers(folder / "broken.zip", error) && !error.empty(),
           "a file that is no zip is refused with its reason");
}

EmulatorEntry core(std::string id, std::string name) {
    EmulatorEntry entry;
    entry.id = std::move(id);
    entry.name = std::move(name);
    entry.systems = {"arcade"};
    entry.core = entry.id + "_libretro";
    entry.romsets = RomsetSource{{"https://example.invalid/" + entry.id + ".dat"}, std::nullopt};
    return entry;
}

void keepsVerdicts(const fs::path& base) {
    const fs::path cache = base / "cache";
    const fs::path folder = base / "kept";
    const RomsetDats dats = RomsetDats::under(cache, nullptr);
    const EmulatorEntry fbneo = core("fbneo", "FinalBurn Neo");
    const EmulatorEntry mame = core("mame", "MAME");
    RomsetDat fbneoDat;
    fbneoDat.addXml(datafile());
    RomsetDat mameDat;
    mameDat.addXml(listxml());
    const auto now = RomsetDats::Clock::now();
    std::string error;
    expect(dats.save(fbneo, fbneoDat, 1, now, error) && dats.save(mame, mameDat, 2, now, error),
           "the listings are kept");
    expect(dats.stamp(fbneo).value_or(opensu::library::roms::DatStamp{}).source == 1,
           "a kept listing's stamp reads");
    expect(dats.refresh(fbneo, now, error), "a fresh listing needs no fetch");
    expect(!dats.refresh(core("other", "Other"), now, error) && !error.empty(),
           "a listing that is not kept and cannot be fetched says so");

    writeZip(folder / "clone.zip", {member("c.1", cloneProgram)});
    const std::vector<const EmulatorEntry*> cores{&fbneo, &mame};
    RomsetLists lists{cache};
    {
        RomsetMatcher matcher{lists};
        matcher.begin(now);
        const std::vector<RomsetVerdict> verdicts = matcher.check(folder / "clone.zip", cores);
        expect(verdicts.size() == 2 && verdicts[0].emulatorId == "fbneo" &&
                   verdicts[0].fit == RomsetFit::Mismatch && verdicts[1].listed == false,
               "each core's verdict is given in the order asked");
        matcher.save();
    }
    expect(fs::is_regular_file(cache / "romsets" / "matches.json"), "the verdicts are kept");
    // A listing that no longer holds the set shows whether a later check reads the kept verdict.
    RomsetDat empty;
    expect(dats.save(fbneo, empty, 1, now, error), "a listing is replaced under the same stamp");
    {
        RomsetMatcher matcher{lists};
        matcher.begin(now);
        expect(matcher.check(folder / "clone.zip", cores)[0].listed,
               "an unchanged zip's verdict is the kept one");
        writeZip(folder / "parent.zip", {member("p.1", program), member("g.1", graphics)});
        expect(!matcher.check(folder / "clone.zip", cores)[0].listed,
               "a parent zip appearing beside it makes the zip match again");
    }
    expect(dats.save(fbneo, fbneoDat, 3, now, error), "a changed listing is kept");
    {
        RomsetMatcher matcher{lists};
        matcher.begin(now);
        expect(matcher.check(folder / "clone.zip", cores)[0].fit == RomsetFit::Matches,
               "a listing with a new stamp matches the zip again");
    }
    {
        RomsetMatcher nowhere;
        nowhere.begin(now);
        expect(nowhere.check(folder / "clone.zip", cores).empty(),
               "a matcher that is nowhere gives no verdicts");
    }
}

/// A host of listings: `/a.dat` and `/b.dat`, and `/slow.dat`, which answers once released.
class ListingHost {
  public:
    [[nodiscard]] std::string base() const {
        return server_.base();
    }
    void release() {
        {
            const std::lock_guard lock{mutex_};
            open_ = true;
        }
        opened_.notify_all();
    }

  private:
    lucent::http::Response answer(const lucent::http::Request& request) {
        const std::string path{request.path()};
        if (path == "/a.dat") {
            return opensu::test::reply(200, datafile());
        }
        if (path == "/b.dat") {
            return opensu::test::reply(200, listxml());
        }
        if (path == "/slow.dat") {
            std::unique_lock lock{mutex_};
            opened_.wait(lock, [this] {
                return open_;
            });
            return opensu::test::reply(200, datafile());
        }
        return opensu::test::reply(404, "");
    }

    std::mutex mutex_;
    std::condition_variable opened_;
    bool open_{false};
    LoopbackServer server_{[this](const lucent::http::Request& request) {
        return answer(request);
    }};
};

template <typename Done> void until(Done done, const char* what) {
    const auto stop = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (!done() && std::chrono::steady_clock::now() < stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    expect(done(), what);
}

void fetchesOffTheScan(const fs::path& base) {
    ListingHost host;
    ReleaseServer releases;
    const std::string lx = buildZip({member("mame0289.xml", listxml(), true)});
    releases.publish("mame", "mame0289", 7, "mame0289lx.zip", lx);
    EmulatorEntry two = core("fbneo", "FinalBurn Neo");
    two.romsets = RomsetSource{{host.base() + "/a.dat", host.base() + "/b.dat"}, std::nullopt};
    EmulatorEntry mame = core("mame", "MAME");
    mame.romsets = RomsetSource{{},
                                ReleaseSource{releases.base() + "/api/mame", "^mame[0-9]+lx\\.zip$",
                                              releases.base() + "/dl/"}};
    EmulatorEntry broken = core("broken", "Broken");
    broken.romsets = RomsetSource{{host.base() + "/missing.dat"}, std::nullopt};

    RomsetLists lists{base / "fetched"};
    const auto now = RomsetLists::Clock::now();
    lists.want(two, now);
    lists.want(mame, now);
    lists.want(broken, now);
    until(
        [&lists] {
            return lists.ended() == 3;
        },
        "each listing asked for is fetched");
    const std::vector<RomsetFetch> fetches = lists.fetches();
    expect(fetches.size() == 3 && fetches[0].id == "fbneo" && fetches[1].id == "mame" &&
               fetches[2].id == "broken",
           "the fetches are listed in the order asked for");
    expect(fetches[0].phase == RomsetFetch::Phase::Done &&
               fetches[0].received == datafile().size() + listxml().size() &&
               fetches[0].total == fetches[0].received && lists.dats().stamp(two).has_value(),
           "a listing of two files is kept, its bytes counted together");
    expect(fetches[1].phase == RomsetFetch::Phase::Done && fetches[1].total == lx.size() &&
               lists.dats().stamp(mame).has_value(),
           "a release's listing is read from its zip, its bytes counted");
    expect(fetches[2].phase == RomsetFetch::Phase::Failed &&
               fetches[2].error.find("HTTP 404") != std::string::npos &&
               fetches[2].retryAt >= now + romsetRetryAfter && !lists.dats().stamp(broken),
           "a failed fetch says why and when it is tried again");

    lists.want(broken, now);
    lists.want(two, now);
    expect(!lists.fetching("broken") && !lists.fetching("fbneo") && lists.ended() == 3,
           "a failed listing waits for its retry time and a kept one is not fetched again");
    lists.want(broken, now + romsetRetryAfter + std::chrono::minutes{1});
    until(
        [&lists] {
            return lists.ended() == 4;
        },
        "a failed listing is fetched again after its retry time");

    EmulatorEntry slow = core("slow", "Slow");
    slow.romsets = RomsetSource{{host.base() + "/slow.dat"}, std::nullopt};
    const fs::path folder = base / "pending";
    writeZip(folder / "clone.zip", {member("c.1", cloneProgram)});
    writeZip(folder / "parent.zip", {member("p.1", program), member("g.1", graphics)});
    const std::vector<const EmulatorEntry*> cores{&slow};
    RomsetMatcher matcher{lists};
    matcher.begin(now);
    expect(matcher.check(folder / "clone.zip", cores)[0].fit == RomsetFit::Pending &&
               lists.fetching("slow"),
           "a scan does not wait for a listing that downloads; its verdict is pending");
    host.release();
    until(
        [&lists] {
            return lists.ended() == 5;
        },
        "the slow listing arrives");
    matcher.begin(now);
    expect(matcher.check(folder / "clone.zip", cores)[0].fit == RomsetFit::Matches,
           "the next scan matches with the listing that arrived");
}

Game arcadeGame() {
    Game game;
    game.id = "rom:/r/Arcade/clone.zip";
    game.source = Source::Rom;
    game.sourceId = "arcade";
    game.emulatorOptions = {
        EmulatorOption{"FinalBurn Neo", true, LaunchSpec{"fbneo", {}}, "fbneo", true, {}, {}, {}},
        EmulatorOption{"MAME 2010", false, {}, "mame2010", true, {}, {}, {}},
        EmulatorOption{"MAME", true, LaunchSpec{"mame", {}}, "mame", true, {}, {}, {}}};
    game.emulator = "FinalBurn Neo";
    game.launch = LaunchSpec{"fbneo", {}};
    return game;
}

RomsetVerdict verdict(std::string id, RomsetFit fit, std::vector<std::string> missing = {}) {
    return RomsetVerdict{std::move(id), fit, true, 4, std::move(missing)};
}

void picksTheEmulator() {
    Game kept = arcadeGame();
    applyRomsets(kept, "clone",
                 {verdict("fbneo", RomsetFit::Matches), verdict("mame", RomsetFit::Matches)});
    expect(kept.emulator == "FinalBurn Neo" && kept.launch.program == "fbneo" &&
               kept.romset == "Matches FinalBurn Neo, MAME",
           "the search's pick stays when it matches, and every match is named");

    Game other = arcadeGame();
    applyRomsets(other, "clone",
                 {verdict("fbneo", RomsetFit::Mismatch, {"a.key"}),
                  verdict("mame2010", RomsetFit::Matches), verdict("mame", RomsetFit::Matches)});
    expect(other.emulator == "MAME" && other.launch.program == "mame" && other.unavailable.empty(),
           "the first matching core that is installed runs it");
    expect(other.emulatorOptions[0].romset == RomsetFit::Mismatch &&
               other.emulatorOptions[0].romsetGap == "lacks 1 of 4 files: a.key",
           "a mismatching core says what the set lacks for it");

    Game download = arcadeGame();
    applyRomsets(download, "clone",
                 {verdict("fbneo", RomsetFit::Mismatch, {"a"}),
                  verdict("mame2010", RomsetFit::Matches),
                  verdict("mame", RomsetFit::Mismatch, {"a", "b"})});
    expect(download.emulator == "MAME 2010" && download.launch.empty() &&
               download.unavailable == "MAME 2010 is not installed",
           "a matching core that is not installed is the one to download");

    Game none = arcadeGame();
    applyRomsets(none, "clone",
                 {verdict("fbneo", RomsetFit::Mismatch, {"a", "b", "c", "d", "e"}),
                  verdict("mame2010", RomsetFit::Mismatch, {"g.1"}),
                  RomsetVerdict{"mame", RomsetFit::Mismatch, false, 0, {}}});
    expect(none.launch.empty() && none.emulator.empty() && none.unavailable == noRomsetMatch &&
               none.romset == noRomsetMatch &&
               none.romsetNearest == "MAME 2010 lacks 1 of 4 files: g.1",
           "with no match the game cannot start and names the nearest core's gap");
    expect(none.emulatorOptions[0].romsetGap == "lacks 5 of 4 files: a, b, c, d and 1 more" &&
               none.emulatorOptions[2].romsetGap == "has no set clone",
           "a long gap names four files, and an unknown set says so");

    Game unchecked = arcadeGame();
    applyRomsets(unchecked, "clone",
                 {verdict("fbneo", RomsetFit::Unchecked), verdict("mame", RomsetFit::Unchecked)});
    expect(unchecked.emulator == "FinalBurn Neo" && unchecked.romset.empty(),
           "unchecked verdicts leave the search's pick");

    Game partly = arcadeGame();
    applyRomsets(partly, "clone",
                 {verdict("fbneo", RomsetFit::Mismatch, {"a"}),
                  verdict("mame2010", RomsetFit::Mismatch, {"a"}),
                  verdict("mame", RomsetFit::Unchecked)});
    expect(partly.romset == noRomsetMatch && !partly.launch.empty(),
           "a core that could not be checked keeps the game playable");

    Game waiting = arcadeGame();
    applyRomsets(waiting, "clone",
                 {verdict("fbneo", RomsetFit::Mismatch, {"a"}),
                  verdict("mame2010", RomsetFit::Pending), verdict("mame", RomsetFit::Unchecked)});
    expect(waiting.romset == checkingRomset && waiting.launch.empty() && waiting.emulator.empty() &&
               waiting.unavailable == romsetListsDownloading &&
               waiting.romsetNearest == "FinalBurn Neo lacks 1 of 4 files: a",
           "with no match yet and a listing downloading, the game waits and says why");
    Game early = arcadeGame();
    applyRomsets(early, "clone",
                 {verdict("fbneo", RomsetFit::Pending), verdict("mame2010", RomsetFit::Pending),
                  verdict("mame", RomsetFit::Matches)});
    expect(early.romset == "Matches MAME" && early.launch.program == "mame",
           "a match runs the game while other listings still download");

    std::vector<Game> pendingGames{waiting, waiting};
    pendingGames[1].id = "rom:/r/Arcade/own.zip";
    SettingLayers pending;
    pending.set(Layer{Layer::Kind::Game, "rom:/r/Arcade/own.zip"}, emulatorKey, "FinalBurn Neo");
    pending.set(Layer{Layer::Kind::System, "arcade"}, emulatorKey, "MAME 2010");
    applyEmulatorPicks(pending, pendingGames);
    expect(pendingGames[0].launch.empty() && pendingGames[0].emulator.empty(),
           "a system's pick waits while the romset is checked");
    expect(pendingGames[1].launch.program == "fbneo",
           "the game's own pick runs it while the romset is checked");

    std::vector<Game> games{other, other};
    games[1].id = "rom:/r/Arcade/own.zip";
    SettingLayers layers;
    layers.set(Layer{Layer::Kind::Game, "rom:/r/Arcade/own.zip"}, emulatorKey, "FinalBurn Neo");
    layers.set(Layer{Layer::Kind::System, "arcade"}, emulatorKey, "FinalBurn Neo");
    applyEmulatorPicks(layers, games);
    expect(games[0].emulator == "MAME" &&
               opensu::library::pickFor(layers, games[0]).source == EmulatorSource::Romset,
           "a system's pick gives way to the core the romset matches, the game's automatic value");
    expect(games[1].emulator == "FinalBurn Neo" && games[1].launch.program == "fbneo",
           "the game's own pick runs it even when the romset does not match");
}

} // namespace

int main() try {
    try {
        const ScratchDir scratch{OPENSU_TEST_SCRATCH, "romset_match"};
        parsesListings();
        matchesZips(scratch.path());
        keepsVerdicts(scratch.path());
        fetchesOffTheScan(scratch.path());
        picksTheEmulator();
        std::puts("romset_match: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
} catch (...) {
    return opensu::test::escaped();
}

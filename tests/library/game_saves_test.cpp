// A game's saves: yuzu's (and Eden's newer) and Ryujinx's layouts read by title id and profile,
// a Flatpak's folders, the built-in runner's files by ROM name, and a copy between two emulators
// backed up first and undone byte for byte, into a slot there or one made as the emulator would
// make it (yuzu's folder, Ryubing's save id and index entry, Kenji-NX's game folder).
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "library/emulator_catalog.hpp"
#include "library/game_saves.hpp"
#include "library/save_copy.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::library;
using namespace opensu::library::saves;
using opensu::test::expect;

constexpr std::string_view title = "010067300059A000";
/// yuzu spells this UUID's folder 947F7CB06C7EEBB86F1D755DD88BF115.
constexpr std::array<unsigned char, 16> edenUuid{0x15, 0xf1, 0x8b, 0xd8, 0x5d, 0x75, 0x1d, 0x6f,
                                                 0xb8, 0xeb, 0x7e, 0x6c, 0xb0, 0x7c, 0x7f, 0x94};
constexpr std::string_view edenUser = "947F7CB06C7EEBB86F1D755DD88BF115";
constexpr std::string_view otherUser = "01B66E243624C906D2819DBDE1A341CC";
constexpr std::string_view ryujinxUser = "00000000000000010000000000000000";

void write(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out{file, std::ios::binary};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string read(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// Every file and folder under `root` with its bytes, relative to it.
std::map<std::string, std::string> tree(const fs::path& root) {
    std::map<std::string, std::string> found;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator{root}) {
        found[fs::relative(entry.path(), root).generic_string()] =
            entry.is_regular_file() ? read(entry.path()) : std::string{"/"};
    }
    return found;
}

void putLe(std::string& bytes, std::size_t at, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        bytes[at + i] = static_cast<char>((value >> (8 * i)) & 0xFFU);
    }
}

std::uint64_t getLe(std::string_view bytes, std::size_t at, std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = size; i > 0; --i) {
        value = (value << 8U) | static_cast<unsigned char>(bytes[at + i - 1]);
    }
    return value;
}

/// LibHac's SaveDataAttribute of a primary save of `type` (1 account, 3 device).
std::string attribute(std::uint64_t program, std::uint64_t userHigh, std::uint8_t type) {
    std::string bytes(0x40, '\0');
    putLe(bytes, 0x00, program, 8);
    putLe(bytes, 0x08, userHigh, 8);
    putLe(bytes, 0x20, type, 1);
    return bytes;
}

/// LibHac's ExtraData0 for a primary save of `type`.
std::string extraData(std::uint64_t program, std::uint64_t userHigh, std::uint8_t type) {
    std::string bytes = attribute(program, userHigh, type);
    bytes.resize(0x200, '\0');
    putLe(bytes, 0x40, program, 8);
    return bytes;
}

/// A KeyValueArchive of SaveDataAttribute to SaveDataIndexerValue, as Ryubing's index keeps it.
std::string archive(const std::vector<std::pair<std::string, std::uint64_t>>& entries) {
    std::string bytes = "IMKV";
    bytes.resize(12, '\0');
    putLe(bytes, 8, entries.size(), 4);
    for (const auto& [key, id] : entries) {
        std::string entry = "IMEN";
        entry.resize(12, '\0');
        putLe(entry, 4, 0x40, 4);
        putLe(entry, 8, 0x40, 4);
        std::string value(0x40, '\0');
        putLe(value, 0, id, 8);
        putLe(value, 0x18, 1, 1);
        bytes += entry;
        bytes += key;
        bytes += value;
    }
    return bytes;
}

/// Each entry's attribute program id and save id, in the archive's order.
std::vector<std::pair<std::uint64_t, std::uint64_t>> archived(std::string_view bytes) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> found;
    std::size_t at = 12;
    for (std::uint64_t i = 0; i < getLe(bytes, 8, 4); ++i) {
        found.emplace_back(getLe(bytes, at + 12, 8), getLe(bytes, at + 12 + 0x40, 8));
        at += 12 + 0x80;
    }
    return found;
}

constexpr std::uint64_t titleNumber = 0x010067300059A000;
constexpr SwitchSaveData saveData{.owner = titleNumber,
                                  .accountSize = 0x400000,
                                  .accountJournal = 0x180000,
                                  .deviceSize = 0x100000,
                                  .deviceJournal = 0x80000};

EmulatorOption installed(std::string name, std::string id) {
    EmulatorOption option;
    option.name = std::move(name);
    option.installed = true;
    option.id = std::move(id);
    return option;
}

struct Rig {
    opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "game-saves"};
    fs::path eden = scratch.path() / "eden";
    fs::path edenConfig = scratch.path() / "eden-config";
    fs::path ryujinx = scratch.path() / "ryujinx";
    fs::path runner = scratch.path() / "saves";

    Rig() {
        std::string profiles(0x650, '\0');
        for (std::size_t i = 0; i < edenUuid.size(); ++i) {
            profiles[0x10 + i] = static_cast<char>(edenUuid[i]);
        }
        write(eden / "system/save/8000000000000010/su/avators/profiles.dat", profiles);
        write(edenConfig / "qt-config.ini",
              "[System]\ncurrent_user\\default=true\ncurrent_user=0\n");
        const fs::path users = eden / "user/save/0000000000000000";
        write(users / edenUser / title / "user/RKB_Profile.SAVE", "eden progress");
        write(users / edenUser / title / "user/RKB1_Slot.SAVE", "eden slot");
        write(users / otherUser / title / "user/RKB_Profile.SAVE", "second profile");
        write(users / "00000000000000000000000000000000" / title / "device.bin", "eden device");
        write(users / edenUser / "0100000000010000" / "other.bin", "another game");

        write(ryujinx / "system/Profiles.json",
              R"({"profiles": [{"user_id": "00000000000000010000000000000000",
                  "account_state": "Open"}]})");
        // Ryubing's layout: the save id names the folder.
        const fs::path account = ryujinx / "user/save/0000000000000003";
        write(account / "ExtraData0", extraData(0x010067300059A000, 1, 1));
        write(account / "ExtraData1", extraData(0x010067300059A000, 1, 1));
        write(account / ".lock", "");
        write(account / "0/user/old.SAVE", "ryujinx progress");
        fs::create_directories(account / "1");
        // A build that files saves by game: the title id names the folder.
        const fs::path device = ryujinx / "user/save" / title / "device";
        write(device / "ExtraData0", extraData(0x010067300059A000, 0, 3));
        write(device / "0/device.bin", "ryujinx device");
        const fs::path elsewhere = ryujinx / "user/save/0000000000000004";
        write(elsewhere / "ExtraData0", extraData(0x0100000000010000, 1, 1));
        fs::create_directories(elsewhere / "0");
    }

    [[nodiscard]] roms::EmulatorCatalog catalog() const {
        const auto store = [](std::string_view layout, const fs::path& folder) {
            return R"("saves": {"layout": ")" + std::string{layout} + R"(", "folder": ")" +
                   folder.string() + R"("})";
        };
        const std::string json =
            R"({"emulators": [{"id": "eden", "name": "Eden", "systems": ["switch"], "config": ")" +
            edenConfig.string() + R"(", )" + store("yuzu", eden) +
            R"(, "launchers": [{"programs": ["eden"]}]}, {"id": "ryujinx", "name": "Ryujinx", )"
            R"("systems": ["switch"], "config": ")" +
            ryujinx.string() + R"(", )" + store("ryujinx", ryujinx) +
            R"json(, "launchers": [{"programs": ["ryujinx"]}]}, {"id": "mgba-core", "name": )json"
            R"json("mGBA (built-in)", "systems": ["gba"], "core": "mgba_libretro"}]})json";
        std::string error;
        return opensu::test::need(roms::EmulatorCatalog::parse(json, "/home/p", error),
                                  "the catalog parses");
    }

    [[nodiscard]] static Game switchGame() {
        Game game;
        game.id = "rom:/r/Kingdom Battle.xci";
        game.source = Source::Rom;
        game.sourceId = "switch";
        game.file = "/r/Kingdom Battle.xci";
        game.titleId = std::string{title};
        game.switchSaveData = saveData;
        game.emulatorOptions = {installed("Eden", "eden"), installed("Ryujinx", "ryujinx")};
        return game;
    }

    /// Ryujinx without its saves of the title, its index naming save 4 only, as Ryubing keeps it.
    void ryujinxWithoutTheGame() const {
        fs::remove_all(ryujinx / "user/save/0000000000000003");
        fs::remove_all(ryujinx / "user/save" / title);
        const std::string index = archive({{attribute(0x0100000000010000, 1, 1), 4}});
        std::string last(8, '\0');
        putLe(last, 0, 4, 8);
        for (const char* side : {"0", "1"}) {
            write(ryujinx / "system/save/8000000000000000" / side / "imkvdb.arc", index);
            write(ryujinx / "system/save/8000000000000000" / side / "lastPublishedId", last);
        }
    }
};

void readsBothLayouts() {
    const Rig rig;
    const std::vector<EmulatorSaves> found = savesOf(Rig::switchGame(), rig.catalog(), rig.runner);
    expect(found.size() == 2 && found[0].emulatorId == "eden" && found[1].emulatorId == "ryujinx",
           "both installed emulators keep a save of the title, in the game's emulator order");
    const SaveSlot* edenAccount = found[0].slotFor(SlotKind::Account);
    expect(edenAccount != nullptr && edenAccount->user == edenUser && edenAccount->current &&
               edenAccount->bytes == 22 && edenAccount->modified.has_value(),
           "Eden's current profile's save, from profiles.dat and qt-config.ini, measured");
    expect(found[0].slots.size() == 3, "Eden keeps two profiles' saves and a device save");
    const SaveSlot* ryuAccount = found[1].slotFor(SlotKind::Account);
    expect(ryuAccount != nullptr && ryuAccount->user == ryujinxUser && ryuAccount->current &&
               ryuAccount->content.front().filename() == "0" && ryuAccount->bytes == 16,
           "Ryujinx's save is found by its ExtraData0, read from its committed folder");
    const SaveSlot* ryuDevice = found[1].slotFor(SlotKind::Device);
    expect(ryuDevice != nullptr && ryuDevice->root.parent_path().filename() == title,
           "a save filed under its title id is found too");

    Game game = Rig::switchGame();
    game.emulatorOptions[1].installed = false;
    expect(savesOf(game, rig.catalog(), rig.runner).size() == 1,
           "an emulator that is not installed is not offered");
    game.titleId.clear();
    expect(!keyOf(game) && savesOf(game, rig.catalog(), rig.runner).empty(),
           "a Switch file without its title id has no saves to find");
}

void readsTheRunnersFiles() {
    const Rig rig;
    write(rig.runner / "gba" / "Minish Cap.srm", std::string(32, 'x'));
    Game game;
    game.id = "rom:/r/Minish Cap.gba";
    game.source = Source::Rom;
    game.sourceId = "gba";
    game.file = "/r/Minish Cap.gba";
    game.emulatorOptions = {installed("mGBA (built-in)", "mgba-core")};
    const std::vector<EmulatorSaves> found = savesOf(game, rig.catalog(), rig.runner);
    expect(found.size() == 1 && found[0].emulatorId == runnerSavesId &&
               found[0].slots.size() == 2 && found[0].bytes() == 32 &&
               found[0].slotFor(SlotKind::Memory)->modified,
           "the runner's .srm is found by the ROM's name under its system's folder");
    const SaveSlot* state = found[0].slotFor(SlotKind::State);
    expect(state != nullptr && !state->modified &&
               state->root == rig.runner / "gba" / "Minish Cap.state",
           "and its state slot where the runner would write it, holding nothing yet");
    game.emulator = "mGBA (built-in)";
    expect(launchedSaves(found, game, rig.catalog()) == &found[0],
           "the saves a game starts with are its emulator's");
    const std::optional<SaveKey> key = keyOf(game);
    expect(key && key->system == "gba" && key->game == "Minish Cap",
           "a ROM's saves are keyed by its system and name");
}

void copiesBackedUpAndUndoes() {
    const Rig rig;
    const Game game = Rig::switchGame();
    const SaveKey key = opensu::test::need(keyOf(game), "the game has a key");
    std::vector<EmulatorSaves> found = savesOf(game, rig.catalog(), rig.runner);
    const fs::path account = found[1].slotFor(SlotKind::Account)->root;
    const fs::path device = found[1].slotFor(SlotKind::Device)->root;
    const std::map<std::string, std::string> before = tree(rig.ryujinx);
    const SaveCopier copier{rig.scratch.path() / "state"};
    expect(!copier.latest(key, "ryujinx"), "no backup before a copy");

    const auto now = std::chrono::system_clock::now();
    CopyProgress progress;
    const std::string failed = copier.copy(key, found[0], found[1], now, progress);
    expect(failed.empty(), "Eden's saves copy over Ryujinx's");
    for (const char* side : {"0", "1"}) {
        expect(read(account / side / "user/RKB_Profile.SAVE") == "eden progress" &&
                   read(account / side / "user/RKB1_Slot.SAVE") == "eden slot" &&
                   !fs::exists(account / side / "user/old.SAVE"),
               "the current profile's files land in the committed and working folders");
    }
    expect(read(device / "0/device.bin") == "eden device", "the device save is copied too");
    expect(read(account / "ExtraData0") == extraData(0x010067300059A000, 1, 1),
           "the save's own attribute stays");
    const std::optional<SaveBackup> backup = copier.latest(key, "ryujinx");
    expect(backup && backup->from == "Eden" &&
               backup->folder.parent_path() ==
                   rig.scratch.path() / "state/save-backups/switch" / title / "ryujinx" &&
               read(backup->folder / "0/0/user/old.SAVE") == "ryujinx progress",
           "Ryujinx's saves were backed up first, under the state dir by console and title");

    expect(progress.total > 0 && progress.done == progress.total,
           "the copy counts every byte it writes");
    expect(copier.undo(key, "ryujinx", progress).empty(), "the copy is undone");
    expect(tree(rig.ryujinx) == before, "undo puts Ryujinx's saves back byte for byte");
    expect(!copier.latest(key, "ryujinx"), "the backup put back is gone");
    expect(!copier.undo(key, "ryujinx", progress).empty(),
           "with no backup there is nothing to undo");

    fs::remove_all(rig.eden / "user/save/0000000000000000");
    found = savesOf(game, rig.catalog(), rig.runner);
    expect(found.size() == 2 && !found[0].holdsFiles() && found[0].home,
           "an emulator without a save of the game is listed, to take one");
    expect(!copier.copy(key, found[0], found[1], now, progress).empty(),
           "it is never a copy's source");
}

void makesTheSlotRyubingWould() {
    const Rig rig;
    rig.ryujinxWithoutTheGame();
    const Game game = Rig::switchGame();
    const SaveKey key = opensu::test::need(keyOf(game), "the game has a key");
    std::vector<EmulatorSaves> found = savesOf(game, rig.catalog(), rig.runner);
    expect(found.size() == 2 && found[1].slots.empty() && !found[1].holdsFiles() && copyable(found),
           "Ryujinx, keeping no save of the game, can take Eden's");
    const std::map<std::string, std::string> before = tree(rig.ryujinx);
    const SaveCopier copier{rig.scratch.path() / "state"};
    CopyProgress progress;
    const std::string failed =
        copier.copy(key, found[0], found[1], std::chrono::system_clock::now(), progress);
    expect(failed.empty(), "Eden's saves copy into a save Ryujinx did not have");
    const fs::path account = rig.ryujinx / "user/save/0000000000000005";
    const std::string extra = read(account / "ExtraData0");
    expect(extra.size() == 0x200 && getLe(extra, 0, 8) == titleNumber && getLe(extra, 8, 8) == 1 &&
               getLe(extra, 0x10, 8) == 0 && getLe(extra, 0x20, 1) == 1 &&
               getLe(extra, 0x40, 8) == titleNumber && getLe(extra, 0x58, 8) == 0x400000 &&
               getLe(extra, 0x60, 8) == 0x180000 && read(account / "ExtraData1") == extra,
           "the account save takes the next save id, its attribute the open profile's");
    expect(read(account / "0/user/RKB_Profile.SAVE") == "eden progress" &&
               read(account / "1/user/RKB_Profile.SAVE") == "eden progress",
           "the files land in its committed and working folders");
    const fs::path index = rig.ryujinx / "system/save/8000000000000000";
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> entries =
        archived(read(index / "0/imkvdb.arc"));
    expect(entries.size() == 3 &&
               entries[0] == std::pair<std::uint64_t, std::uint64_t>{0x0100000000010000, 4} &&
               entries[1].first == titleNumber && entries[2].first == titleNumber &&
               read(index / "1/imkvdb.arc") == read(index / "0/imkvdb.arc") &&
               getLe(read(index / "0/lastPublishedId"), 0, 8) == 6,
           "each new save has its index entry, in LibHac's attribute order");
    found = savesOf(game, rig.catalog(), rig.runner);
    expect(found[1].slotFor(SlotKind::Account) != nullptr &&
               found[1].slotFor(SlotKind::Account)->current &&
               found[1].slotFor(SlotKind::Device) != nullptr,
           "Ryujinx's new saves read back as the open profile's and the device's");
    expect(copier.undo(key, "ryujinx", progress).empty() && tree(rig.ryujinx) == before,
           "undo removes the saves made and puts the index back byte for byte");
}

void makesTheSlotKenjiWould() {
    const Rig rig;
    rig.ryujinxWithoutTheGame();
    write(rig.ryujinx / "user/save-layout.lock", "");
    const Game game = Rig::switchGame();
    const SaveKey key = opensu::test::need(keyOf(game), "the game has a key");
    const std::vector<EmulatorSaves> found = savesOf(game, rig.catalog(), rig.runner);
    const std::map<std::string, std::string> before = tree(rig.ryujinx);
    const SaveCopier copier{rig.scratch.path() / "state"};
    CopyProgress progress;
    expect(copier.copy(key, found[0], found[1], std::chrono::system_clock::now(), progress).empty(),
           "Eden's saves copy into Kenji-NX's");
    const fs::path game_folder = rig.ryujinx / "user/save" / title;
    expect(read(game_folder / ryujinxUser / "0/user/RKB_Profile.SAVE") == "eden progress" &&
               getLe(read(game_folder / ryujinxUser / "ExtraData0"), 0x20, 1) == 1 &&
               read(game_folder / "device/0/device.bin") == "eden device" &&
               getLe(read(game_folder / "device/ExtraData0"), 0x20, 1) == 3 &&
               getLe(read(game_folder / "device/ExtraData0"), 0x58, 8) == 0x100000,
           "Kenji-NX's saves are filed by game, profile and device, with no index entry");
    expect(read(rig.ryujinx / "system/save/8000000000000000/0/imkvdb.arc") ==
               before.at("system/save/8000000000000000/0/imkvdb.arc"),
           "Kenji-NX's index is left to it");
    expect(copier.undo(key, "ryujinx", progress).empty() && tree(rig.ryujinx) == before,
           "undo removes the game's folder it made");
}

void makesTheSlotYuzuWould() {
    const Rig rig;
    const Game game = Rig::switchGame();
    const SaveKey key = opensu::test::need(keyOf(game), "the game has a key");
    fs::remove_all(rig.eden / "user/save/0000000000000000" / edenUser / title);
    fs::remove_all(rig.eden / "user/save/0000000000000000/00000000000000000000000000000000");
    const std::vector<EmulatorSaves> found = savesOf(game, rig.catalog(), rig.runner);
    expect(found[0].slotFor(SlotKind::Account) != nullptr &&
               !found[0].slotFor(SlotKind::Account)->current,
           "Eden's current profile keeps no save of the game, another profile does");
    const std::map<std::string, std::string> before = tree(rig.eden);
    const SaveCopier copier{rig.scratch.path() / "state"};
    CopyProgress progress;
    expect(copier.copy(key, found[1], found[0], std::chrono::system_clock::now(), progress).empty(),
           "Ryujinx's saves copy into Eden's");
    const fs::path users = rig.eden / "user/save/0000000000000000";
    expect(read(users / edenUser / title / "user/old.SAVE") == "ryujinx progress" &&
               read(users / otherUser / title / "user/RKB_Profile.SAVE") == "second profile" &&
               read(users / "00000000000000000000000000000000" / title / "device.bin") ==
                   "ryujinx device",
           "the current profile's save and the device save are made under yuzu's NAND path");
    expect(copier.undo(key, "eden", progress).empty() && tree(rig.eden) == before,
           "undo removes the folders it made");
}

void readsEdensNewerLayoutAndFlatpaks() {
    const Rig rig;
    std::string raw;
    for (const unsigned char byte : edenUuid) {
        raw += std::format("{:02x}", byte);
    }
    write(rig.eden / "user/save/account" / raw / title / "0/user/new.SAVE", "newer place");
    write(rig.eden / "user/save/device" / title / "0/device.bin", "newer device");
    const std::vector<EmulatorSaves> found = savesOf(Rig::switchGame(), rig.catalog(), rig.runner);
    const SaveSlot* account = found[0].slotFor(SlotKind::Account);
    const SaveSlot* device = found[0].slotFor(SlotKind::Device);
    expect(account != nullptr && account->user == edenUser &&
               account->content.front() == rig.eden / "user/save/account" / raw / title / "0" &&
               account->bytes == 11,
           "Eden's newer account folder wins over the old one for its profile");
    expect(device != nullptr &&
               device->content.front() == rig.eden / "user/save/device" / title / "0",
           "and its newer device folder");

    expect(roms::flatpakFolder("/home/p/.local/share/eden/nand", "/home/p", "dev.eden_emu.eden") ==
                   "/home/p/.var/app/dev.eden_emu.eden/data/eden/nand" &&
               roms::flatpakFolder("/home/p/.config/Ryujinx/bis", "/home/p",
                                   "io.github.ryubing.Ryujinx") ==
                   "/home/p/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis" &&
               roms::flatpakFolder("/srv/eden", "/home/p", "dev.eden_emu.eden").empty(),
           "a Flatpak keeps ~/.config and ~/.local/share under ~/.var/app/<app id>");
    const std::string json =
        R"({"emulators": [{"id": "eden", "name": "Eden", "systems": ["switch"], )"
        R"("config": "~/.config/eden", "saves": {"layout": "yuzu", )"
        R"("folder": "~/.local/share/eden/nand"}, "launchers": [{"programs": ["eden"], )"
        R"("flatpak": "dev.eden_emu.eden"}]}]})";
    const fs::path home = rig.scratch.path() / "home";
    std::string error;
    const roms::EmulatorCatalog catalog = opensu::test::need(
        roms::EmulatorCatalog::parse(json, home, error), "the Flatpak catalog parses");
    const fs::path sandbox = home / ".var/app/dev.eden_emu.eden";
    fs::create_directories(sandbox / "data/eden");
    fs::copy(rig.eden, sandbox / "data/eden/nand", fs::copy_options::recursive);
    fs::create_directories(sandbox / "config");
    fs::copy(rig.edenConfig, sandbox / "config/eden", fs::copy_options::recursive);
    Game game = Rig::switchGame();
    game.emulatorOptions = {installed("Eden", "eden")};
    expect(savesOf(game, catalog, rig.runner).front().slots.empty(),
           "a host install reads the host's folders");
    game.emulatorOptions[0].flatpak = true;
    const std::vector<EmulatorSaves> sandboxed = savesOf(game, catalog, rig.runner);
    expect(sandboxed.front().home && sandboxed.front().home->folder == sandbox / "data/eden/nand" &&
               sandboxed.front().slotFor(SlotKind::Account) != nullptr &&
               sandboxed.front().slotFor(SlotKind::Account)->current,
           "the Flatpak's saves and current profile are read inside its sandbox folders");
}

} // namespace

int main() try {
    readsBothLayouts();
    readsTheRunnersFiles();
    copiesBackedUpAndUndoes();
    makesTheSlotRyubingWould();
    makesTheSlotKenjiWould();
    makesTheSlotYuzuWould();
    readsEdensNewerLayoutAndFlatpaks();
    std::puts("game_saves: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

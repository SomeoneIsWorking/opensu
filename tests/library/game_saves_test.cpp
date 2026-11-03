// A game's saves: yuzu's and Ryujinx's layouts read by title id and profile, the built-in runner's
// files by ROM name, and a copy between two emulators backed up first and undone byte for byte.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
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

/// LibHac's ExtraData0 for a primary save of `type` (1 account, 3 device).
std::string extraData(std::uint64_t program, std::uint64_t userHigh, std::uint8_t type) {
    std::string bytes(0x200, '\0');
    putLe(bytes, 0x00, program, 8);
    putLe(bytes, 0x08, userHigh, 8);
    putLe(bytes, 0x20, type, 1);
    putLe(bytes, 0x40, program, 8);
    return bytes;
}

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
        game.emulatorOptions = {installed("Eden", "eden"), installed("Ryujinx", "ryujinx")};
        return game;
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
               found[0].slots.size() == 1 && found[0].slots[0].kind == SlotKind::Memory &&
               found[0].bytes() == 32,
           "the runner's .srm is found by the ROM's name under its system's folder");
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
    const std::string failed = copier.copy(key, found[0], found[1], now);
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

    expect(copier.undo(key, "ryujinx").empty(), "the copy is undone");
    expect(tree(rig.ryujinx) == before, "undo puts Ryujinx's saves back byte for byte");
    expect(!copier.latest(key, "ryujinx"), "the backup put back is gone");
    expect(!copier.undo(key, "ryujinx").empty(), "with no backup there is nothing to undo");

    fs::remove_all(rig.eden / "user/save/0000000000000000");
    found = savesOf(game, rig.catalog(), rig.runner);
    expect(found.size() == 1, "an emulator without a save of the game is not offered");
}

} // namespace

int main() try {
    readsBothLayouts();
    readsTheRunnersFiles();
    copiesBackedUpAndUndoes();
    std::puts("game_saves: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// A folder share as two openSUs see it: saves sealed by one open for the other with the same
// passphrase, never with another, and a changed or swapped file is refused; a memory card travels
// as one.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "library/game_saves.hpp"
#include "retro/resume_file.hpp"
#include "save_sync/folder_share.hpp"
#include "save_sync/save_bundle.hpp"
#include "save_sync/share_target.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using save_sync::SaveTarget;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::unique_ptr<save_sync::ShareTarget> targetOn(const fs::path& folder, std::string passphrase) {
    return std::make_unique<save_sync::ShareTarget>(
        "share:" + folder.string(), "Saves", std::make_unique<save_sync::FolderShare>(folder),
        std::move(passphrase));
}

std::string bytesOf(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void aResumeStateRidesOnlyWithItsSave(const fs::path& root) {
    using library::saves::SlotKind;
    const auto slot = [](SlotKind kind, const fs::path& file) {
        return library::saves::SaveSlot{.kind = kind,
                                        .user = {},
                                        .current = true,
                                        .root = file,
                                        .content = {file},
                                        .format = kind == SlotKind::Memory
                                                      ? library::saves::SaveFormat::SaveRam
                                                      : library::saves::SaveFormat::AsIs,
                                        .bytes = 0,
                                        .modified = std::chrono::system_clock::now()};
    };
    const fs::path folder = root / "resume";
    fs::create_directories(folder);
    const fs::path memory = folder / "game.srm";
    const fs::path resume = folder / "game.resume";
    const auto writeSave = [&memory](std::string_view bytes) {
        std::ofstream{memory, std::ios::binary} << bytes;
    };
    writeSave("save RAM");
    std::string error;
    const std::array state{std::byte{1}};
    expect(retro::writeResume(resume,
                              retro::ResumeHeader{.core = "mgba",
                                                  .coreLabel = "mGBA",
                                                  .coreVersion = "1",
                                                  .saved = std::chrono::system_clock::now(),
                                                  .saveRam = retro::saveRamCrc("save RAM")},
                              state, error),
           "a resume state is written");
    const library::saves::EmulatorSaves both{
        .emulatorId = "runner",
        .emulatorName = "Runner",
        .slots = {slot(SlotKind::Memory, memory), slot(SlotKind::Resume, resume)},
        .home = std::nullopt,
        .processHint = {}};
    const library::saves::EmulatorSaves alone{.emulatorId = "runner",
                                              .emulatorName = "Runner",
                                              .slots = {slot(SlotKind::Resume, resume)},
                                              .home = std::nullopt,
                                              .processHint = {}};
    const auto sent = [&error](const library::saves::EmulatorSaves& saves) {
        const std::optional<save_sync::SaveBundle> bundle = save_sync::bundleOf(saves, error);
        return bundle ? bundle->files.size() : std::size_t{99};
    };
    const std::optional<save_sync::SaveBundle> together = save_sync::bundleOf(both, error);
    const std::optional<save_sync::SaveBundle> read =
        together ? save_sync::readBundle(save_sync::write(*together)) : std::nullopt;
    expect(together && together->files.size() == 2, "a resume state rides with its save");
    expect(read && together && read->hash() == together->hash(),
           "and reads back on the other side");
    writeSave("another save");
    expect(sent(both) == 1, "one the save moved on from stays behind");
    expect(sent(alone) == 0, "and one with no save beside it never goes alone");
}

void aCardTravelsAsACard(const fs::path& root) {
    using library::saves::SlotKind;
    const fs::path card = root / "card" / "Crash Twinsanity (v1.00).ps2";
    fs::create_directories(card.parent_path());
    std::ofstream{card, std::ios::binary} << "memory card";
    const library::saves::EmulatorSaves saves{
        .emulatorId = "built-in",
        .emulatorName = "Built-in",
        .slots = {library::saves::SaveSlot{.kind = SlotKind::Card,
                                           .user = {},
                                           .current = false,
                                           .root = card,
                                           .content = {card},
                                           .format = library::saves::SaveFormat::AsIs,
                                           .bytes = 11,
                                           .modified = std::chrono::system_clock::now()}},
        .home = std::nullopt,
        .processHint = {}};
    std::string error;
    const std::optional<save_sync::SaveBundle> bundle = save_sync::bundleOf(saves, error);
    const std::optional<save_sync::SaveBundle> read =
        bundle ? save_sync::readBundle(save_sync::write(*bundle)) : std::nullopt;
    expect(read && read->files.size() == 1 && read->files[0].kind == SlotKind::Card &&
               read->files[0].bytes == "memory card",
           "a memory card is bundled and read back as a card");
    const library::saves::EmulatorSaves staged =
        save_sync::staged(*read, root / "card-staged", "Share");
    expect(staged.slotFor(SlotKind::Card) != nullptr &&
               bytesOf(staged.slotFor(SlotKind::Card)->content.front()) == "memory card",
           "and staged as the card slot a copy takes");
}

void sealedSavesOpenOnlyWithThePassphrase(const fs::path& root) {
    const fs::path folder = root / "share";
    fs::create_directories(folder);
    const library::saves::SaveKey kirby{.system = "gba", .game = "Kirby", .profile = "player"};
    const save_sync::SaveBundle bundle{
        {save_sync::BundleFile{.kind = library::saves::SlotKind::Memory,
                               .path = {},
                               .bytes = "the save RAM of a cartridge",
                               .modified = std::chrono::system_clock::now()}}};
    const save_sync::SaveStatus status{
        .counter = 3, .hash = bundle.hash(), .bytes = bundle.bytes(), .modified = 1700000000};

    const auto attic = targetOn(folder, "correct horse");
    expect(attic->status(std::stop_token{}, kirby).kind == SaveTarget::Held::Kind::Saves &&
               attic->status(std::stop_token{}, kirby).status.hash.empty(),
           "an empty share holds no saves of the game");
    std::string error;
    expect(attic->give(std::stop_token{}, kirby, bundle, status, false, error) ==
               SaveTarget::Given::Kept,
           "the share keeps what it is given");

    const auto den = targetOn(folder, "correct horse");
    expect(den->status(std::stop_token{}, kirby).status == status,
           "another openSU reads the same status");
    const std::optional<save_sync::SaveBundle> fetched =
        den->fetch(std::stop_token{}, kirby, error);
    expect(fetched && fetched->hash() == bundle.hash(), "and the same saves");

    std::string sealed;
    for (const fs::directory_entry& entry : fs::directory_iterator{folder / "opensu-saves"}) {
        sealed += bytesOf(entry.path());
    }
    expect(sealed.find("the save RAM of a cartridge") == std::string::npos,
           "the saves are not in the share in the clear");

    const auto study = targetOn(folder, "another passphrase");
    const SaveTarget::Held refused = study->status(std::stop_token{}, kirby);
    expect(refused.kind == SaveTarget::Held::Kind::Failed &&
               refused.problem.find("passphrase") != std::string::npos,
           "another passphrase does not open the share");

    fs::path statusFile;
    fs::path bundleFile;
    for (const fs::directory_entry& entry : fs::directory_iterator{folder / "opensu-saves"}) {
        if (entry.path().extension() == ".status") {
            statusFile = entry.path();
        } else if (entry.path().extension() == ".bundle") {
            bundleFile = entry.path();
        }
    }
    fs::copy_file(bundleFile, statusFile, fs::copy_options::overwrite_existing);
    expect(targetOn(folder, "correct horse")->status(std::stop_token{}, kirby).kind ==
               SaveTarget::Held::Kind::Failed,
           "a file put in another's place does not open");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "share-target"};
    sealedSavesOpenOnlyWithThePassphrase(scratch.path());
    aResumeStateRidesOnlyWithItsSave(scratch.path());
    aCardTravelsAsACard(scratch.path());
    std::printf("share_target: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

// A folder share as two openSUs see it: saves sealed by one open for the other with the same
// passphrase, never with another, and a changed or swapped file is refused.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include "save_sync/folder_share.hpp"
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
    std::printf("share_target: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

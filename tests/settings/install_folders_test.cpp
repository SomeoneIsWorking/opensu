// The install folder resolver: a store's own folder, else the default, else the store's choice;
// and which folders can take an install.
#include "settings/install_folders.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <unistd.h>

#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::Source;
using opensu::settings::InstallFolders;
using opensu::test::expect;

void resolves() {
    InstallFolders folders;
    for (const Source store : opensu::settings::installStores) {
        expect(!folders.effective(store), "with nothing set, a store keeps its own choice");
    }
    folders.setDefault("/games");
    expect(folders.effective(Source::Steam) == fs::path{"/games"} &&
               folders.effective(Source::Epic) == fs::path{"/games"} &&
               folders.effective(Source::Gog) == fs::path{"/games"},
           "the default applies to every store");
    folders.setStore(Source::Epic, "/epic");
    expect(folders.effective(Source::Epic) == fs::path{"/epic"} &&
               folders.effective(Source::Steam) == fs::path{"/games"},
           "a store's own folder wins for that store only");
    folders.setStore(Source::Epic, "");
    expect(folders.effective(Source::Epic) == fs::path{"/games"},
           "an empty override makes the store follow the default again");
    expect(folders.storeFolder(Source::Epic).empty(), "and holds no folder of its own");
    folders.setDefault("");
    expect(!folders.effective(Source::Epic), "with no default either, the store chooses");
}

void refusals(const fs::path& root) {
    fs::create_directories(root / "ok");
    expect(opensu::settings::refusal(root / "ok").empty(), "a writable folder is accepted");
    expect(!opensu::settings::refusal(root / "missing").empty(), "a missing folder is refused");
    std::ofstream{root / "file"} << "x";
    expect(!opensu::settings::refusal(root / "file").empty(), "a file is refused");
    expect(!opensu::settings::refusal("relative/dir").empty(), "a relative path is refused");
    fs::create_directories(root / "locked");
    fs::permissions(root / "locked", fs::perms::owner_read | fs::perms::owner_exec);
    expect(!opensu::settings::refusal(root / "locked").empty() || geteuid() == 0,
           "a folder that cannot be written is refused");
    fs::permissions(root / "locked", fs::perms::owner_all);
}

void resolvesWithTheStoresOwn() {
    using opensu::library::Source;
    using opensu::settings::Destination;
    using opensu::settings::FolderOrigin;
    const opensu::settings::StoreFolders own{{Source::Epic, "/home/p/Games"},
                                             {Source::Gog, "/data/opensu/gog-games"}};
    InstallFolders folders;
    expect(folders.resolve(Source::Epic, own) ==
               Destination{"/home/p/Games", FolderOrigin::StoreOwn},
           "with nothing set, a store's own folder is shown as its own");
    expect(folders.resolve(Source::Steam, own) == Destination{{}, FolderOrigin::StoreOwn},
           "a store whose folder is not known here has none");
    folders.setDefault("/games");
    expect(folders.resolve(Source::Gog, own) == Destination{"/games", FolderOrigin::Default},
           "the default wins over the store's own, marked as the default");
    folders.setStore(Source::Gog, "/fast");
    expect(folders.resolve(Source::Gog, own) == Destination{"/fast", FolderOrigin::Store} &&
               folders.effective(Source::Gog) == fs::path{"/fast"},
           "an override wins, the same folder the install reads");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "install-folders";
    fs::remove_all(root);
    fs::create_directories(root);
    resolves();
    resolvesWithTheStoresOwn();
    refusals(root);
    fs::remove_all(root);
    std::printf("install_folders: all checks passed\n");
    return 0;
}

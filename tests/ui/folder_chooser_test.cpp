// The folder chooser over a real directory tree: its folders, going in and up, and a missing start.
#include "folder_chooser.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu::ui;
using opensu::test::expect;

void listsFoldersOnly(const fs::path& root) {
    FolderChooser chooser;
    chooser.open(root);
    expect(chooser.current() == root && chooser.hasParent() && chooser.problem().empty(),
           "opens on the start");
    std::vector<std::string> names;
    for (const FolderEntry& entry : chooser.folders()) {
        names.push_back(entry.name);
    }
    expect(names == std::vector<std::string>{"alpha", "Beta"},
           "folders sort without case, files and dotfolders are skipped");
}

void goingInAndUp(const fs::path& root) {
    FolderChooser chooser;
    chooser.open(root);
    chooser.enter(chooser.folders().front().path);
    expect(chooser.current() == root / "alpha" && chooser.folders().empty(),
           "a folder is gone into");
    expect(chooser.up() && chooser.current() == root, "up returns");
    FolderChooser top;
    top.open("/");
    expect(!top.hasParent() && !top.up() && top.current() == "/", "nothing is above the root");
}

void startBelowAMissingFolderFallsBackUp(const fs::path& root) {
    FolderChooser chooser;
    chooser.open(root / "gone" / "deeper");
    expect(chooser.current() == root, "a missing start opens on its nearest folder above");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "folder_chooser";
    fs::remove_all(root);
    fs::create_directories(root / "alpha");
    fs::create_directories(root / "Beta");
    fs::create_directories(root / ".hidden");
    std::ofstream out{root / "file.txt"};
    out.close();
    listsFoldersOnly(root);
    goingInAndUp(root);
    startBelowAMissingFolderFallsBackUp(root);
    fs::remove_all(root);
    std::printf("folder_chooser: all checks passed\n");
    return 0;
}

// The folder chooser over a real directory tree: its entries, going in and up, and what pressing
// each entry asks for.
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
    chooser.open("Pick", root);
    expect(chooser.isOpen() && chooser.current() == root && chooser.title() == "Pick",
           "opens on the start");
    const std::vector<FolderEntry>& entries = chooser.entries();
    expect(entries.size() >= 3 && entries[0].kind == FolderEntryKind::Use &&
               entries[1].kind == FolderEntryKind::Type &&
               entries[2].kind == FolderEntryKind::Parent,
           "Use, Type and Parent lead");
    std::vector<std::string> folders;
    for (const FolderEntry& entry : entries) {
        if (entry.kind == FolderEntryKind::Folder) {
            folders.push_back(entry.label);
        }
    }
    expect(folders == std::vector<std::string>{"alpha", "Beta"},
           "folders sort without case, files and dotfolders are skipped");
}

void clearEntryOnlyWhenAsked(const fs::path& root) {
    FolderChooser plain;
    plain.open("Pick", root);
    FolderChooser clearing;
    clearing.open("Pick", root, "Follow the default");
    expect(clearing.entries().size() == plain.entries().size() + 1 &&
               clearing.entries()[2].kind == FolderEntryKind::Clear,
           "a clear label adds the entry");
    clearing.move(2);
    expect(clearing.press().clear, "pressing it asks to drop the setting");
}

void pressingMoves(const fs::path& root) {
    FolderChooser chooser;
    chooser.open("Pick", root);
    const FolderPress use = chooser.press();
    expect(use.chosen && *use.chosen == root, "Use chooses the shown folder");
    expect(chooser.move(1) && chooser.press().type, "Type asks to type");
    chooser.focusEntry(3);
    expect(chooser.entries()[3].kind == FolderEntryKind::Folder, "the first folder entry");
    const FolderPress in = chooser.press();
    expect(!in.chosen && !in.type && chooser.current() == root / "alpha" && chooser.focus() == 0,
           "a folder is gone into with the first entry focused");
    expect(chooser.up() && chooser.current() == root, "up returns");
    chooser.focusEntry(2);
    chooser.press();
    expect(chooser.current() == root.parent_path(), "the Parent entry goes up");
}

void startBelowAMissingFolderFallsBackUp(const fs::path& root) {
    FolderChooser chooser;
    chooser.open("Pick", root / "gone" / "deeper");
    expect(chooser.current() == root, "a missing start opens on its nearest folder above");
    chooser.close();
    expect(!chooser.isOpen(), "closed");
}

void layoutScrollsAndHits() {
    const Rect frame{0, 0, 1920, 1080};
    const FolderChooserLayout top = layoutFolderChooser(frame, 2.25f, FolderWalk{60, 0});
    const FolderChooserLayout bottom = layoutFolderChooser(frame, 2.25f, FolderWalk{60, 59});
    expect(top.rows.size() == 60, "a box per entry");
    expect(bottom.rows[59].y >= bottom.list.y &&
               bottom.rows[59].bottom() <= bottom.list.bottom() + 0.5f,
           "the focused last entry is whole in the list");
    expect(!bottom.rowAt(bottom.list.centreX(), bottom.rows[0].centreY()),
           "a scrolled-off entry is not hit");
    const auto hit = top.rowAt(top.rows[1].centreX(), top.rows[1].centreY());
    expect(hit && *hit == 1, "an entry's centre hits it");
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
    clearEntryOnlyWhenAsked(root);
    pressingMoves(root);
    startBelowAMissingFolderFallsBackUp(root);
    layoutScrollsAndHits();
    fs::remove_all(root);
    std::printf("folder_chooser: all checks passed\n");
    return 0;
}

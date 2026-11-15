// `opensu games`: a shortcut and a program added through the records, adding again keeping the
// entry (retitled when asked), the list, removal by id and by file, and the refusals.
#include "library/own_games_command.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
namespace own = opensu::library::own;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

struct Ran {
    int status;
    std::string out;
    std::string err;
};

Ran run(const own::Records& records, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int status = own::runGamesCommand(args, records, out, err);
    return Ran{.status = status, .out = out.str(), .err = err.str()};
}

void addsListsRemoves(const fs::path& dir) {
    fs::create_directories(dir / "Nebula Reset");
    const fs::path shortcut = dir / "devoid.desktop";
    std::ofstream{shortcut} << "[Desktop Entry]\nType=Application\nName=Devoid\nExec=godot --path "
                               ".\nPath=/g/devoid\n";
    const fs::path program = dir / "Nebula Reset" / "NebulaReset.x86_64";
    std::ofstream{program} << "x";
    fs::permissions(program, fs::perms::owner_all);
    const fs::path data = dir / "notes.txt";
    std::ofstream{data} << "x";
    const own::Records records{own::recordsFile(dir / "data")};

    const Ran added = run(records, {"add", shortcut.string()});
    expect(added.status == 0 && added.out == "own:1\tDevoid\n", "a shortcut is added by its name");
    const Ran again = run(records, {"add", shortcut.string(), "--title", "Devoid (dev)"});
    expect(again.status == 0 && again.out == "own:1\tDevoid (dev)\n" && records.all().size() == 1 &&
               records.all().front().title == "Devoid (dev)",
           "adding it again keeps its entry and takes the new title");
    const Ran programAdded = run(records, {"add", program.string()});
    expect(programAdded.status == 0 && programAdded.out == "own:2\tNebula Reset\n",
           "a program is titled from its files");

    const Ran listed = run(records, {"list"});
    expect(listed.status == 0 && listed.out == "own:1\tDevoid (dev)\t" + shortcut.string() +
                                                   "\nown:2\tNebula Reset\t" + program.string() +
                                                   "\n",
           "the list gives each id, title and file");

    expect(run(records, {"add", data.string()}).status == 1, "a file no game is, is refused");
    expect(run(records, {"remove", "own:1"}).status == 0 &&
               run(records, {"remove", program.string()}).status == 0 && records.all().empty(),
           "a game is removed by its id or its file");
    expect(run(records, {"remove", "own:1"}).status == 1, "a game not there is refused");
    expect(run(records, {}).status == 2 && run(records, {"add"}).status == 2 &&
               !run(records, {"frobnicate"}).err.empty(),
           "a command it does not know shows the usage");
}

} // namespace

int main() try {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "own-games-command"};
    addsListsRemoves(scratch.path());
    std::printf("own_games_command: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

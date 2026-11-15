#include "own_games_command.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "game.hpp"
#include "game_identify.hpp"

namespace opensu::library::own {
namespace {

namespace fs = std::filesystem;

/// `file` as the records keep a picked one: absolute and normal.
fs::path keptPath(const fs::path& file) {
    return fs::absolute(file).lexically_normal();
}

/// The entry `named` names: its game id (`own:<id>`) or its file.
std::optional<Entry> entryNamed(const Records& records, const std::string& named) {
    const std::vector<Entry> all = records.all();
    const auto found = std::ranges::find_if(all, [&named](const Entry& entry) {
        return gameOf(entry).id == named ||
               (!entry.file.empty() && entry.file == keptPath(fs::path{named}));
    });
    return found == all.end() ? std::nullopt : std::optional<Entry>{*found};
}

/// The title the files give `entry`, as "Add a game" guesses it before the store is asked.
std::string titleOf(const Entry& entry) {
    const identify::Guess guess = identify::identify(
        entry.file,
        [](const std::string& /*term*/) {
            return std::vector<identify::StoreApp>{};
        },
        entry.title);
    return guess.title.empty() ? entry.file.stem().string() : guess.title;
}

int add(std::span<const std::string> args, const Records& records, std::ostream& out,
        std::ostream& err) {
    std::optional<std::string> title;
    std::optional<fs::path> file;
    for (std::size_t at = 0; at < args.size(); ++at) {
        if (args[at] == "--title" && at + 1 < args.size()) {
            title = args[++at];
        } else if (!file && !args[at].starts_with("--")) {
            file = keptPath(args[at]);
        } else {
            err << "games add: unexpected " << args[at] << "\n" << gamesUsage;
            return 2;
        }
    }
    if (!file || (title && title->empty())) {
        err << gamesUsage;
        return 2;
    }
    if (const std::optional<Entry> kept = entryNamed(records, file->string())) {
        if (title && *title != kept->title) {
            static_cast<void>(records.rename(kept->id, *title));
        }
        out << gameOf(*kept).id << '\t' << title.value_or(kept->title) << '\n';
        return 0;
    }
    std::string refused;
    std::optional<Entry> entry = pickedEntry(*file, refused);
    if (!entry) {
        err << "games add: " << refused << '\n';
        return 1;
    }
    entry->title = title.value_or(titleOf(*entry));
    entry->id = records.add(*entry);
    out << gameOf(*entry).id << '\t' << entry->title << '\n';
    return 0;
}

int remove(std::span<const std::string> args, const Records& records, std::ostream& out,
           std::ostream& err) {
    if (args.size() != 1) {
        err << gamesUsage;
        return 2;
    }
    const std::optional<Entry> kept = entryNamed(records, args.front());
    if (!kept || !records.remove(kept->id)) {
        err << "games remove: no game added as " << args.front() << '\n';
        return 1;
    }
    out << gameOf(*kept).id << '\t' << kept->title << '\n';
    return 0;
}

} // namespace

int runGamesCommand(std::span<const std::string> args, const Records& records, std::ostream& out,
                    std::ostream& err) try {
    const std::string_view verb = args.empty() ? std::string_view{} : args.front();
    const std::span<const std::string> rest = args.empty() ? args : args.subspan(1);
    if (verb == "add") {
        return add(rest, records, out, err);
    }
    if (verb == "remove") {
        return remove(rest, records, out, err);
    }
    if (verb == "list" && rest.empty()) {
        for (const Entry& entry : records.all()) {
            out << gameOf(entry).id << '\t' << entry.title << '\t' << entry.file.string() << '\n';
        }
        return 0;
    }
    err << gamesUsage;
    return 2;
} catch (const std::runtime_error& failure) {
    err << "games: " << failure.what() << '\n';
    return 1;
}

} // namespace opensu::library::own

#include "own_games.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "fileio/atomic_write.hpp"
#include "launch/command.hpp"
#include "store_builds.hpp"

namespace opensu::library::own {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::string lowered(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool executable(const fs::path& file) {
    return !launch::resolveExecutable(file.string(), {}).empty();
}

/// The interpreter a script's `#!` line names, with its one argument; `sh` when it names none.
std::vector<std::string> interpreterOf(const fs::path& script) {
    std::ifstream in{script};
    std::string line;
    if (!std::getline(in, line) || !line.starts_with("#!")) {
        return {"sh"};
    }
    std::vector<std::string> words;
    std::string word;
    for (const char c : line.substr(2)) {
        if (c == ' ' || c == '\t' || c == '\r') {
            if (!word.empty()) {
                words.push_back(std::move(word));
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(std::move(word));
    }
    return words.empty() ? std::vector<std::string>{"sh"} : words;
}

std::string originKey(Origin origin) {
    switch (origin) {
    case Origin::SteamShortcut:
        return "steam-shortcut";
    case Origin::Lutris:
        return "lutris";
    case Origin::File:
        break;
    }
    return "file";
}

Origin originOf(const std::string& key) {
    if (key == "steam-shortcut") {
        return Origin::SteamShortcut;
    }
    return key == "lutris" ? Origin::Lutris : Origin::File;
}

/// The launch of a file of `kind`; `unavailable` says why there is none.
void fileLaunch(const Entry& entry, Kind kind, Game& game) {
    const std::string name = entry.file.filename().string();
    LaunchSpec launch{.program = entry.file.string(),
                      .args = entry.args,
                      .workingDirectory = entry.workingDirectory.empty() ? entry.file.parent_path()
                                                                         : entry.workingDirectory};
    switch (kind) {
    case Kind::Windows:
        game.windowsInstall = !windowsBuildsNative;
        break;
    case Kind::Script:
        if (!executable(entry.file)) {
            std::vector<std::string> interpreter = interpreterOf(entry.file);
            launch.program = interpreter.front();
            interpreter.erase(interpreter.begin());
            interpreter.push_back(entry.file.string());
            launch.args.insert(launch.args.begin(), interpreter.begin(), interpreter.end());
        }
        break;
    case Kind::AppImage:
    case Kind::Native:
        if (!executable(entry.file)) {
            game.unavailable =
                name + " is not executable; make it executable (chmod +x) to play it";
            return;
        }
        break;
    }
    game.processHint = name;
    game.launch = std::move(launch);
}

} // namespace

std::optional<Kind> kindOf(const fs::path& file) {
    std::error_code error;
    if (!fs::is_regular_file(file, error)) {
        return std::nullopt;
    }
    const std::string extension = lowered(file.extension().string());
    if (extension == ".exe") {
        return Kind::Windows;
    }
    if (extension == ".sh") {
        return Kind::Script;
    }
    if (extension == ".appimage") {
        return Kind::AppImage;
    }
    return executable(file) ? std::optional{Kind::Native} : std::nullopt;
}

std::string refusal(const fs::path& file) {
    std::error_code error;
    if (!fs::is_regular_file(file, error)) {
        return file.filename().string() + " is not a file";
    }
    if (!kindOf(file)) {
        return file.filename().string() +
               " is not a program openSU can start: pick an executable, a .sh script, an AppImage "
               "or a Windows .exe";
    }
    return {};
}

fs::path recordsFile(const fs::path& dataDir) {
    return dataDir / "own-games.json";
}

Records::Records(fs::path file) : file_{std::move(file)} {
}

Records::Document Records::read() const {
    Document read;
    std::error_code ec;
    if (!fs::exists(file_, ec)) {
        return read;
    }
    std::ifstream in{file_};
    const json document = json::parse(in, nullptr, false);
    if (!document.is_object() || !document.contains("games") || !document["games"].is_array()) {
        throw std::runtime_error{file_.string() + " is not a list of games"};
    }
    read.next = document.value("next", 1LL);
    for (const json& game : document["games"]) {
        if (!game.is_object() || !game.contains("id") || !game.contains("title")) {
            continue;
        }
        Entry entry{.id = game.value("id", ""),
                    .title = game.value("title", ""),
                    .file = game.value("file", ""),
                    .args = game.value("args", std::vector<std::string>{}),
                    .workingDirectory = game.value("workingDirectory", ""),
                    .origin = originOf(game.value("origin", "")),
                    .originId = game.value("originId", ""),
                    .steamAppId = game.value("steamAppId", "")};
        read.entries.push_back(std::move(entry));
    }
    return read;
}

std::vector<Entry> Records::all() const {
    return read().entries;
}

std::optional<Entry> Records::find(std::string_view id) const {
    const std::vector<Entry> entries = all();
    const auto found = std::ranges::find(entries, id, &Entry::id);
    return found == entries.end() ? std::nullopt : std::optional{*found};
}

std::string Records::add(Entry entry) const {
    Document document = read();
    entry.id = std::to_string(document.next++);
    document.entries.push_back(std::move(entry));
    write(document);
    return document.entries.back().id;
}

bool Records::remove(std::string_view id) const {
    Document document = read();
    if (std::erase_if(document.entries, [id](const Entry& entry) {
            return entry.id == id;
        }) == 0) {
        return false;
    }
    write(document);
    return true;
}

bool Records::setFile(std::string_view id, const fs::path& file) const {
    Document document = read();
    const auto found = std::ranges::find(document.entries, id, &Entry::id);
    if (found == document.entries.end()) {
        return false;
    }
    found->file = file;
    write(document);
    return true;
}

bool Records::rename(std::string_view id, const std::string& title) const {
    Document document = read();
    const auto found = std::ranges::find(document.entries, id, &Entry::id);
    if (found == document.entries.end()) {
        return false;
    }
    found->title = title;
    write(document);
    return true;
}

void Records::write(const Document& document) const {
    json games = json::array();
    for (const Entry& entry : document.entries) {
        json game = {{"id", entry.id},
                     {"title", entry.title},
                     {"file", entry.file.string()},
                     {"origin", originKey(entry.origin)}};
        if (!entry.args.empty()) {
            game["args"] = entry.args;
        }
        if (!entry.workingDirectory.empty()) {
            game["workingDirectory"] = entry.workingDirectory.string();
        }
        if (!entry.originId.empty()) {
            game["originId"] = entry.originId;
        }
        if (!entry.steamAppId.empty()) {
            game["steamAppId"] = entry.steamAppId;
        }
        games.push_back(std::move(game));
    }
    std::string error;
    if (!fileio::writeWhole(
            file_, json{{"next", document.next}, {"games", std::move(games)}}.dump(2), error)) {
        throw std::runtime_error{error};
    }
}

Game gameOf(const Entry& entry) {
    Game game;
    game.id = key(Source::Own) + ":" + entry.id;
    game.source = Source::Own;
    game.sourceId = entry.id;
    game.title = entry.title;
    game.file = entry.file;
    game.artworkKey = entry.steamAppId;
    game.installed = true;
    if (entry.origin == Origin::Lutris) {
        game.launch =
            LaunchSpec{.program = "lutris", .args = {"lutris:rungameid/" + entry.originId}};
        return game;
    }
    std::error_code error;
    if (!fs::is_regular_file(entry.file, error)) {
        game.unavailable =
            entry.file.string() + " is not there any more; pick its file again with Change file";
        return game;
    }
    const std::optional<Kind> kind = kindOf(entry.file);
    if (!kind) {
        game.unavailable = refusal(entry.file);
        return game;
    }
    fileLaunch(entry, *kind, game);
    return game;
}

std::vector<Game> Provider::list() {
    const std::vector<Entry> entries = records_.all();
    if (entries.empty()) {
        throw SourceAbsent{"no games added"};
    }
    std::vector<Game> games;
    games.reserve(entries.size());
    for (const Entry& entry : entries) {
        games.push_back(gameOf(entry));
    }
    return games;
}

} // namespace opensu::library::own

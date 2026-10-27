#include "lutris_games.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <sqlite3.h>

namespace opensu::library::own {
namespace {

namespace fs = std::filesystem;

struct CloseDatabase {
    void operator()(sqlite3* database) const noexcept {
        sqlite3_close(database);
    }
};
struct FinishStatement {
    void operator()(sqlite3_stmt* statement) const noexcept {
        sqlite3_finalize(statement);
    }
};

/// `path` as the path of an SQLite file URI: `%`, `?` and `#` escaped.
std::string uriPath(const std::string& path) {
    std::string out;
    for (const char c : path) {
        if (c == '%') {
            out += "%25";
        } else if (c == '?') {
            out += "%3f";
        } else if (c == '#') {
            out += "%23";
        } else {
            out += c;
        }
    }
    return out;
}

std::string column(sqlite3_stmt* statement, int index) {
    const unsigned char* text = sqlite3_column_text(statement, index);
    return text == nullptr ? std::string{} : std::string{reinterpret_cast<const char*>(text)};
}

} // namespace

std::vector<Entry> lutrisGames(const fs::path& lutrisData) {
    const fs::path file = lutrisData / "pga.db";
    std::error_code error;
    if (!fs::is_regular_file(file, error)) {
        return {};
    }
    // immutable=1: no lock, journal or -shm file is made next to Lutris's database.
    const std::string uri = "file:" + uriPath(file.string()) + "?immutable=1";
    sqlite3* opened = nullptr;
    const int status =
        sqlite3_open_v2(uri.c_str(), &opened, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    const std::unique_ptr<sqlite3, CloseDatabase> database{opened};
    if (status != SQLITE_OK) {
        throw std::runtime_error{"cannot open " + file.string() + ": " + sqlite3_errstr(status)};
    }
    sqlite3_stmt* prepared = nullptr;
    if (sqlite3_prepare_v2(database.get(),
                           "SELECT id, name FROM games WHERE installed = 1 ORDER BY name", -1,
                           &prepared, nullptr) != SQLITE_OK) {
        throw std::runtime_error{"cannot read " + file.string() + ": " +
                                 sqlite3_errmsg(database.get())};
    }
    const std::unique_ptr<sqlite3_stmt, FinishStatement> statement{prepared};
    std::vector<Entry> entries;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        std::string name = column(statement.get(), 1);
        if (name.empty()) {
            continue;
        }
        entries.push_back(Entry{.id = {},
                                .title = std::move(name),
                                .file = {},
                                .args = {},
                                .workingDirectory = {},
                                .origin = Origin::Lutris,
                                .originId = column(statement.get(), 0)});
    }
    return entries;
}

} // namespace opensu::library::own

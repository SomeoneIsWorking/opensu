#include "launch_files.hpp"

#include <system_error>
#include <utility>

#include "fileio/atomic_write.hpp"
#include "fileio/read_whole.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

fs::path marked(const fs::path& file, std::string_view suffix) {
    fs::path mark = file;
    mark += suffix;
    return mark;
}

bool present(const fs::path& file) {
    std::error_code missing;
    return fs::exists(file, missing);
}

/// Undoes what the launch did to `marks.file`; false when it did nothing there.
bool putBack(const LaunchFiles::Marks& marks) {
    std::error_code failed;
    if (present(marks.kept)) {
        fs::rename(marks.kept, marks.file, failed);
    } else if (present(marks.absent)) {
        fs::remove(marks.file, failed);
        if (!failed) {
            fs::remove(marks.absent, failed);
        }
    } else {
        return false;
    }
    if (failed) {
        lucent::error("emulator", "cannot put the player's {} back: {}", marks.file.native(),
                      failed.message());
    }
    return true;
}

/// Keeps the player's `file` aside, or marks that there is none; false, logged, when it cannot.
bool keep(const fs::path& file) {
    std::error_code failed;
    if (present(file)) {
        fs::rename(file, marked(file, LaunchFiles::keptSuffix), failed);
    } else {
        std::string error;
        if (!fileio::writeWhole(marked(file, LaunchFiles::absentSuffix), "", error)) {
            lucent::warn("emulator", "{} stays the player's: {}", file.string(), error);
            return false;
        }
    }
    if (failed) {
        lucent::warn("emulator", "{} stays the player's: {}", file.string(), failed.message());
        return false;
    }
    return true;
}

} // namespace

std::optional<RunFile> runFileOf(const fs::path& file,
                                 const std::function<std::string(std::string_view)>& run) {
    std::string player;
    if (present(file)) {
        std::string error;
        std::optional<std::string> read = fileio::readWhole(file, error);
        if (!read) {
            lucent::warn("emulator", "{} stays the player's: {}", file.string(), error);
            return std::nullopt;
        }
        player = std::move(*read);
    }
    std::string bytes = run(player);
    if (bytes == player) {
        return std::nullopt;
    }
    return RunFile{.path = file, .bytes = std::move(bytes)};
}

LaunchFiles::Marks::Marks(const fs::path& player)
    : file(player), kept(marked(player, keptSuffix)), absent(marked(player, absentSuffix)) {
}

void LaunchFiles::recover(const std::vector<fs::path>& files) {
    for (const fs::path& file : files) {
        const Marks marks{file};
        if (present(marks.kept) || present(marks.absent)) {
            lucent::info("emulator", "putting back the player's {} a crashed run left set",
                         file.string());
            static_cast<void>(putBack(marks));
        }
    }
}

LaunchFiles::LaunchFiles(const std::vector<RunFile>& files) {
    for (const RunFile& file : files) {
        Marks marks{file.path};
        if (!keep(file.path)) {
            continue;
        }
        std::string error;
        if (!fileio::writeWhole(file.path, file.bytes, error)) {
            lucent::warn("emulator", "{} stays the player's: {}", file.path.string(), error);
            static_cast<void>(putBack(marks));
            continue;
        }
        set_.push_back(std::move(marks));
        lucent::info("emulator", "{} set for the launch", file.path.string());
    }
}

LaunchFiles::~LaunchFiles() {
    for (const Marks& marks : set_) {
        static_cast<void>(putBack(marks));
    }
    if (!set_.empty()) {
        lucent::info("emulator", "the player's {} settings files put back", set_.size());
    }
}

} // namespace opensu::library

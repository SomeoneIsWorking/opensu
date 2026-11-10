#include "launch_files.hpp"

#include <system_error>

#include "fileio/atomic_write.hpp"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

fs::path marked(const fs::path& file, std::string_view suffix) {
    fs::path mark = file;
    mark += suffix;
    return mark;
}

bool exists(const fs::path& file) {
    std::error_code missing;
    return fs::exists(file, missing);
}

/// Undoes what the launch did to `file`; false when it did nothing there.
bool putBack(const fs::path& file) {
    std::error_code failed;
    const fs::path kept = marked(file, LaunchFiles::keptSuffix);
    const fs::path absent = marked(file, LaunchFiles::absentSuffix);
    if (exists(kept)) {
        fs::rename(kept, file, failed);
    } else if (exists(absent)) {
        fs::remove(file, failed);
        if (!failed) {
            fs::remove(absent, failed);
        }
    } else {
        return false;
    }
    if (failed) {
        lucent::error("emulator", "cannot put the player's {} back: {}", file.string(),
                      failed.message());
    }
    return true;
}

/// Keeps the player's `file` aside, or marks that there is none; false, logged, when it cannot.
bool keep(const fs::path& file) {
    std::error_code failed;
    if (exists(file)) {
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

void LaunchFiles::recover(const std::vector<fs::path>& files) {
    for (const fs::path& file : files) {
        if (exists(marked(file, keptSuffix)) || exists(marked(file, absentSuffix))) {
            lucent::info("emulator", "putting back the player's {} a crashed run left set",
                         file.string());
            static_cast<void>(putBack(file));
        }
    }
}

LaunchFiles::LaunchFiles(const std::vector<RunFile>& files) {
    for (const RunFile& file : files) {
        if (!keep(file.path)) {
            continue;
        }
        std::string error;
        if (!fileio::writeWhole(file.path, file.bytes, error)) {
            lucent::warn("emulator", "{} stays the player's: {}", file.path.string(), error);
            static_cast<void>(putBack(file.path));
            continue;
        }
        set_.push_back(file.path);
        lucent::info("emulator", "{} set for the launch", file.path.string());
    }
}

LaunchFiles::~LaunchFiles() {
    for (const fs::path& file : set_) {
        static_cast<void>(putBack(file));
    }
    if (!set_.empty()) {
        lucent::info("emulator", "the player's {} settings files put back", set_.size());
    }
}

} // namespace opensu::library

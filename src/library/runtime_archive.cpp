#include "runtime_archive.hpp"

#include <system_error>

#include "launch/command.hpp"
#include "lucent/content.h"
#include "lucent/log.h"

namespace opensu::library {
namespace {

namespace fs = std::filesystem;

void removeQuietly(const fs::path& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
}

/// The file's digest of `kind` as lower-case hex; nothing with `error`.
std::optional<std::string> digestOf(const fs::path& file, RuntimeArchive::Digest kind,
                                    std::string& error) {
    if (kind == RuntimeArchive::Digest::Sha256) {
        const auto digest = lucent::content::sha256_file(file, error);
        return digest ? std::optional{lucent::content::sha256_hex(*digest)} : std::nullopt;
    }
    const auto digest = lucent::content::sha512_file(file, error);
    return digest ? std::optional{lucent::content::sha512_hex(*digest)} : std::nullopt;
}

} // namespace

std::optional<fs::path> installArchive(const net::WebClient& web, const RuntimeArchive& archive,
                                       const fs::path& into, const std::stop_token& stop,
                                       const net::WebClient::Progress& progress,
                                       const std::function<void(ArchiveStage)>& stage,
                                       std::string& error) {
    const auto unsafe = [](const std::string& name) {
        return name.empty() || name.find('/') != std::string::npos || name.starts_with('.');
    };
    if (unsafe(archive.folder) || unsafe(archive.installAs) || unsafe(archive.name)) {
        error = "the release names an unsafe file or folder " + archive.name;
        return std::nullopt;
    }
    const fs::path partial = into / ".partial";
    const fs::path file = partial / archive.name;
    const fs::path staging = partial / (archive.installAs + ".unpacking");
    removeQuietly(partial);
    std::error_code ec;
    fs::create_directories(staging, ec);
    if (ec) {
        error = "cannot make " + staging.string() + ": " + ec.message();
        return std::nullopt;
    }
    const auto fail = [&partial](std::string& why, std::string text) {
        removeQuietly(partial);
        why = std::move(text);
        return std::nullopt;
    };

    stage(ArchiveStage::Downloading);
    lucent::info("wine", "downloading {} from {}", archive.name, archive.url);
    if (!web.download(archive.url, file, progress, error)) {
        return fail(error, error == "cancelled" ? error : "the download failed: " + error);
    }
    const std::uint64_t size = fs::file_size(file, ec);
    if (ec || (archive.size != 0 && size != archive.size)) {
        return fail(error, "the download is " + std::to_string(size) + " bytes, the release says " +
                               std::to_string(archive.size));
    }

    stage(ArchiveStage::Checking);
    const std::optional<std::string> seen = digestOf(file, archive.digest, error);
    if (!seen) {
        return fail(error, error);
    }
    const char* kind = archive.digest == RuntimeArchive::Digest::Sha256 ? "SHA-256" : "SHA-512";
    if (*seen != archive.hex) {
        lucent::warn("wine", "{}: {} {} is not the published {}", archive.name, kind, *seen,
                     archive.hex);
        return fail(error, std::string{"the download's "} + kind +
                               " is not the one the release publishes");
    }
    lucent::info("wine", "{} matches its published {} {}", archive.name, kind, *seen);

    stage(ArchiveStage::Unpacking);
    std::string lastLine;
    const std::optional<int> status = launch::runStreaming(
        "tar", {"-xf", file.string(), "-C", staging.string(), "--no-same-owner"},
        [&lastLine](std::string_view line) {
            lastLine = line;
        },
        stop);
    if (stop.stop_requested()) {
        return fail(error, "cancelled");
    }
    if (!status || *status != 0) {
        return fail(error, "tar could not unpack " + archive.name +
                               (lastLine.empty() ? std::string{} : ": " + lastLine));
    }
    const fs::path unpacked = staging / archive.folder;
    if (!fs::is_directory(unpacked, ec)) {
        return fail(error, archive.name + " holds no " + archive.folder + " folder");
    }
    fs::path target = into / archive.installAs;
    if (fs::exists(target, ec)) {
        // Installed meanwhile; a folder in place may be in use and is never replaced.
        removeQuietly(partial);
        return target;
    }
    fs::rename(unpacked, target, ec);
    if (ec) {
        return fail(error, "cannot put " + target.string() + " in place: " + ec.message());
    }
    removeQuietly(partial);
    lucent::info("wine", "{} unpacked to {}", archive.name, target.string());
    return target;
}

} // namespace opensu::library

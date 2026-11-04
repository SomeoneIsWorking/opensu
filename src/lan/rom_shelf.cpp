#include "rom_shelf.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>
#include <utility>

#include "clock/file_time.hpp"
#include "lucent/log.h"
#include "peer_crypto.hpp"
#include "relative_path.hpp"
#include "secrets/secret.hpp"

namespace opensu::lan {
namespace {

namespace fs = std::filesystem;

/// How much of a file is read at once while it is hashed.
constexpr std::size_t hashPiece = std::size_t{1} << 20;

std::string keyOf(wire::RomKind kind, std::uint32_t root, std::string_view path) {
    return std::string{kind == wire::RomKind::Bios ? "bios " : "rom "} + std::to_string(root) +
           " " + std::string{path};
}

/// `file`'s size and write time; nothing when it is not a regular file.
std::optional<std::pair<std::uint64_t, std::int64_t>> stampOf(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) {
        return std::nullopt;
    }
    const std::uintmax_t size = fs::file_size(file, ec);
    const fs::file_time_type written = fs::last_write_time(file, ec);
    if (ec) {
        return std::nullopt;
    }
    return std::pair{static_cast<std::uint64_t>(size), clock::sinceEpoch(written).count()};
}

/// The files of the game at `entry` (a file, or every file under a folder) as paths inside
/// `root`, sorted; nothing when one could not be named across the LAN.
std::optional<std::vector<wire::RomFile>> filesOf(const fs::path& root, const fs::path& entry) {
    std::vector<wire::RomFile> files;
    std::error_code ec;
    const auto add = [&](const fs::path& file) {
        const std::string path = file.lexically_relative(root).generic_string();
        const std::uintmax_t size = fs::file_size(file, ec);
        if (ec || path.empty() || !staysInside(path)) {
            return false;
        }
        files.push_back(wire::RomFile{path, static_cast<std::uint64_t>(size)});
        return true;
    };
    if (fs::is_regular_file(entry, ec)) {
        if (!add(entry)) {
            return std::nullopt;
        }
        return files;
    }
    for (fs::recursive_directory_iterator walk{entry, ec}, end; !ec && walk != end;
         walk.increment(ec)) {
        if (walk->is_regular_file(ec) && !add(walk->path())) {
            return std::nullopt;
        }
    }
    if (ec || files.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(files, {}, &wire::RomFile::path);
    return files;
}

} // namespace

RomShelf::RomShelf()
    : hasher_{[this](const std::stop_token& stop) {
          hashAll(stop);
      }} {
}

RomShelf::~RomShelf() {
    hasher_.request_stop();
    wake_.notify_all();
}

void RomShelf::share(Contents contents) {
    const std::lock_guard lock{mutex_};
    contents_ = std::move(contents);
    built_ = false;
}

void RomShelf::build() {
    if (built_) {
        return;
    }
    built_ = true;
    consoles_.clear();
    files_.clear();
    std::map<std::pair<std::string, std::string>, wire::RomConsole> grouped;
    for (const library::Game& game : contents_.games) {
        if (game.source != library::Source::Rom || !game.installed || game.file.empty()) {
            continue;
        }
        for (std::size_t root = 0; root < contents_.roots.size(); ++root) {
            const fs::path relative = game.file.lexically_relative(contents_.roots[root]);
            auto step = relative.begin();
            if (relative.empty() || step == relative.end() || *step == "..") {
                continue;
            }
            const fs::path folder = *step;
            if (++step == relative.end()) {
                break;
            }
            const fs::path entry = folder / *step;
            std::optional<std::vector<wire::RomFile>> files =
                filesOf(contents_.roots[root], contents_.roots[root] / entry);
            if (!files) {
                lucent::info("lan", "not offering {}: a file of it cannot be named over the LAN",
                             game.title);
                break;
            }
            const auto index = static_cast<std::uint32_t>(root);
            for (const wire::RomFile& file : *files) {
                files_.emplace(keyOf(wire::RomKind::Rom, index, file.path),
                               Listed{contents_.roots[root] / file.path, file.bytes});
            }
            wire::RomConsole& console = grouped[{game.sourceId, folder.generic_string()}];
            console.system = game.sourceId;
            console.folder = folder.generic_string();
            console.games.push_back(
                wire::RomGame{game.title, index, entry.generic_string(), std::move(*files)});
            break;
        }
    }
    for (auto& [key, console] : grouped) {
        std::ranges::sort(console.games, {}, &wire::RomGame::title);
        const auto bios = contents_.bios.find(console.system);
        for (const std::string& path :
             bios != contents_.bios.end() ? bios->second : std::vector<std::string>{}) {
            const fs::path file = contents_.biosFolder / path;
            const std::optional<std::pair<std::uint64_t, std::int64_t>> stamp = stampOf(file);
            if (!stamp || !staysInside(path) || path.empty()) {
                continue;
            }
            console.bios.push_back(wire::RomFile{path, stamp->first});
            files_.emplace(keyOf(wire::RomKind::Bios, 0, path), Listed{file, stamp->first});
        }
        consoles_.push_back(std::move(console));
    }
    lucent::info("lan", "offering {} ROM files of {} consoles to import", files_.size(),
                 consoles_.size());
}

std::vector<wire::RomConsole> RomShelf::listing() {
    const std::lock_guard lock{mutex_};
    build();
    return consoles_;
}

std::optional<RomShelf::Listed> RomShelf::find(const wire::RomAsk& ask) {
    const std::lock_guard lock{mutex_};
    build();
    const auto found =
        files_.find(keyOf(ask.kind, ask.kind == wire::RomKind::Bios ? 0 : ask.root, ask.path));
    if (found == files_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<wire::RomFacts> RomShelf::factsOf(const wire::RomAsk& ask) {
    const std::optional<Listed> listed = find(ask);
    if (!listed) {
        return std::nullopt;
    }
    const std::optional<std::pair<std::uint64_t, std::int64_t>> stamp = stampOf(listed->file);
    if (!stamp) {
        return std::nullopt;
    }
    wire::RomFacts facts{stamp->first, {}};
    {
        const std::lock_guard lock{mutex_};
        const auto hashed = hashes_.find(listed->file);
        if (hashed != hashes_.end() && hashed->second.bytes == stamp->first &&
            hashed->second.written == stamp->second) {
            facts.sha256 = hashed->second.sha256;
            return facts;
        }
        if (std::ranges::find(toHash_, listed->file) == toHash_.end()) {
            toHash_.push_back(listed->file);
        }
    }
    wake_.notify_all();
    return facts;
}

std::optional<std::string> RomShelf::chunkOf(const wire::RomAsk& ask, std::string& error) {
    const std::optional<Listed> listed = find(ask);
    if (!listed) {
        error = "no ROM folder here lists that file";
        return std::nullopt;
    }
    const std::optional<std::pair<std::uint64_t, std::int64_t>> stamp = stampOf(listed->file);
    if (!stamp || ask.offset > stamp->first) {
        error = stamp ? "that is past the file's end" : "the file is gone";
        return std::nullopt;
    }
    const std::uint64_t wanted = ask.length == 0 ? wire::romChunkBytes : ask.length;
    const std::uint64_t length = std::min({wanted, wire::romChunkBytes, stamp->first - ask.offset});
    std::ifstream in{listed->file, std::ios::binary};
    std::string bytes(static_cast<std::size_t>(length), '\0');
    in.seekg(static_cast<std::streamoff>(ask.offset));
    in.read(bytes.data(), static_cast<std::streamsize>(length));
    if (!in || static_cast<std::uint64_t>(in.gcount()) != length) {
        error = "the file cannot be read";
        return std::nullopt;
    }
    return bytes;
}

void RomShelf::hashAll(const std::stop_token& stop) {
    std::vector<char> piece(hashPiece);
    while (!stop.stop_requested()) {
        fs::path file;
        {
            std::unique_lock lock{mutex_};
            if (!wake_.wait(lock, stop, [this] {
                    return !toHash_.empty();
                })) {
                return;
            }
            file = toHash_.front();
        }
        const std::optional<std::pair<std::uint64_t, std::int64_t>> stamp = stampOf(file);
        std::ifstream in{file, std::ios::binary};
        crypto::Sha256 hash;
        std::uint64_t read = 0;
        while (in && !stop.stop_requested()) {
            in.read(piece.data(), static_cast<std::streamsize>(piece.size()));
            const auto got = static_cast<std::size_t>(in.gcount());
            hash.add(std::string_view{piece.data(), got});
            read += got;
        }
        const crypto::Bytes digest = hash.finish();
        const std::lock_guard lock{mutex_};
        std::erase(toHash_, file);
        if (stamp && read == stamp->first && in.eof()) {
            hashes_[file] = Hashed{stamp->first, stamp->second, secrets::hexText(digest)};
        } else {
            lucent::warn("lan", "{} cannot be hashed to send", file.string());
        }
    }
}

} // namespace opensu::lan

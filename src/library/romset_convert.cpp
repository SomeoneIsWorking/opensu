#include "romset_convert.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <map>
#include <vector>

#include "lucent/log.h"
#include "romset_match.hpp"
#include "zip/zip_archive.hpp"
#include "zip/zip_writer.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// The zips of a conversion, each file's entry by what a listing calls it.
class Sources {
  public:
    explicit Sources(const RomTree& tree) : tree_{&tree} {
    }

    [[nodiscard]] bool add(const fs::path& location, std::string& error) {
        std::unique_ptr<RomFile> file = tree_->open(location);
        if (file == nullptr) {
            error = location.string() + " cannot be opened";
            return false;
        }
        const std::optional<std::vector<zip::Entry>> entries =
            readZipEntries(*file, location.filename().string(), error);
        if (!entries) {
            return false;
        }
        for (const zip::Entry& entry : *entries) {
            held_.emplace(entry.crc32, entry.size);
            files_.try_emplace(entry.crc32, Member{entry, zips_.size()});
        }
        zips_.push_back(std::move(file));
        return true;
    }

    [[nodiscard]] const HeldFiles& held() const noexcept {
        return held_;
    }

    /// The file of this CRC, kept until another is asked for: a recipe's neighbours share one.
    [[nodiscard]] const std::string* file(std::uint32_t crc32, std::string& error) {
        if (loaded_ && loadedCrc_ == crc32) {
            return &bytes_;
        }
        const auto found = files_.find(crc32);
        if (found == files_.end()) {
            error = std::format("no file of CRC {:08x}", crc32);
            return nullptr;
        }
        const zip::Entry& entry = found->second.entry;
        RomFile& zip = *zips_.at(found->second.zip);
        const std::optional<std::string> header =
            zip.read(entry.localHeaderOffset, zip::localHeaderSize);
        const std::optional<std::uint64_t> offset =
            header ? zip::dataOffset(entry, *header, error) : std::nullopt;
        const std::optional<std::string> data =
            offset ? zip.read(*offset, entry.compressedSize) : std::nullopt;
        std::optional<std::string> bytes = data ? zip::extract(entry, *data, error) : std::nullopt;
        if (!bytes) {
            error = entry.name + " cannot be read: " + error;
            return nullptr;
        }
        bytes_ = *std::move(bytes);
        loaded_ = true;
        loadedCrc_ = crc32;
        return &bytes_;
    }

  private:
    struct Member {
        zip::Entry entry;
        std::size_t zip{0};
    };

    const RomTree* tree_;
    std::vector<std::unique_ptr<RomFile>> zips_;
    std::map<std::uint32_t, Member> files_;
    HeldFiles held_;
    std::string bytes_;
    std::uint32_t loadedCrc_{0};
    bool loaded_{false};
};

/// The bytes a recipe makes.
std::optional<std::string> built(const PlannedRom& planned, Sources& sources, std::string& error) {
    std::string bytes;
    bytes.reserve(planned.rom->size);
    for (const ConversionPiece& piece : planned.pieces) {
        if (piece.source == 0) {
            bytes.append(piece.length, static_cast<char>(piece.fill));
            continue;
        }
        const std::string* file = sources.file(piece.source, error);
        if (file == nullptr) {
            return std::nullopt;
        }
        for (std::uint32_t n = 0; n < piece.length; ++n) {
            bytes.push_back((*file)[piece.offset + std::size_t{n} * piece.stride]);
        }
    }
    return bytes;
}

std::optional<std::string> bytesOf(const PlannedRom& planned, Sources& sources,
                                   std::string& error) {
    if (planned.key != nullptr) {
        return cps2KeyFile(*planned.key);
    }
    if (!planned.pieces.empty()) {
        return built(planned, sources, error);
    }
    const std::string* file = sources.file(planned.rom->crc32, error);
    return file == nullptr ? std::nullopt : std::optional<std::string>{*file};
}

} // namespace

bool RomsetConverter::write(const RomConversion& conversion, const EmulatorEntry& entry,
                            const fs::path& file, std::string& error) const {
    const std::optional<RomsetDat> dat =
        dats_.load(entry, RomsetDat::Wanted{conversion.target}, error);
    const DatSet* set = dat ? dat->find(conversion.target) : nullptr;
    if (!dat || set == nullptr) {
        error = entry.name + " does not list " + conversion.target + ": " + error;
        return false;
    }
    Sources sources{*tree_};
    for (const fs::path& zip : conversion.zips) {
        if (!sources.add(zip, error)) {
            return false;
        }
    }
    const ConversionPlan plan = conversions_.plan(needsOf(*set, *dat).roms, sources.held());
    if (!plan.missing.empty()) {
        error = conversion.target + " lacks " + plan.missing.front();
        return false;
    }
    std::ofstream out{file, std::ios::binary | std::ios::trunc};
    zip::Writer writer{out};
    HeldFiles written;
    for (const PlannedRom& planned : plan.roms) {
        if (!written.emplace(planned.rom->crc32, planned.rom->size).second) {
            continue;
        }
        const std::optional<std::string> bytes = bytesOf(planned, sources, error);
        if (!bytes) {
            return false;
        }
        if (bytes->size() != planned.rom->size || zip::crc32Of(*bytes) != planned.rom->crc32) {
            error = planned.rom->name + " came out unlike its listing";
            return false;
        }
        if (!writer.add(planned.rom->name, *bytes, error)) {
            return false;
        }
    }
    return writer.finish(error);
}

std::optional<fs::path> RomsetConverter::build(const RomConversion& conversion,
                                               const EmulatorEntry& entry,
                                               std::string& error) const {
    if (tree_ == nullptr) {
        error = "romsets cannot be converted here";
        return std::nullopt;
    }
    const std::string stamp = std::format(
        "{:08x}", zip::crc32Of(conversion.stamp + "/" + entry.id + "/" + conversion.target));
    const fs::path folder = dir_ / stamp;
    fs::path file = folder / (conversion.target + ".zip");
    std::error_code ec;
    if (fs::is_regular_file(file, ec)) {
        fs::last_write_time(file, fs::file_time_type::clock::now(), ec);
        lucent::info("roms", "{} kept at {}", conversion.target, file.string());
        return file;
    }
    fs::create_directories(folder, ec);
    const fs::path part = folder / (conversion.target + ".part");
    const bool wrote = write(conversion, entry, part, error);
    if (wrote) {
        fs::rename(part, file, ec);
    }
    if (!wrote || ec) {
        if (ec) {
            error = ec.message();
        }
        fs::remove(part, ec);
        return std::nullopt;
    }
    lucent::info("roms", "{} rebuilt at {}", conversion.target, file.string());
    trim(file);
    return file;
}

void RomsetConverter::trim(const fs::path& keep) const {
    struct Kept {
        fs::path file;
        fs::file_time_type used;
        std::uintmax_t size{0};
    };
    std::vector<Kept> kept;
    std::uintmax_t total = 0;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::recursive_directory_iterator{dir_, ec}) {
        if (item.is_regular_file(ec)) {
            kept.push_back(Kept{item.path(), item.last_write_time(ec), item.file_size(ec)});
            total += kept.back().size;
        }
    }
    std::ranges::sort(kept, {}, &Kept::used);
    for (const Kept& oldest : kept) {
        if (total <= convertedLimit) {
            break;
        }
        if (oldest.file != keep) {
            fs::remove(oldest.file, ec);
            fs::remove(oldest.file.parent_path(), ec);
            total -= oldest.size;
        }
    }
}

} // namespace opensu::library::roms

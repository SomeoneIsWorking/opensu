#include "romset_dats.hpp"

#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <span>
#include <system_error>

#include "emulator_release.hpp"
#include "fileio/atomic_write.hpp"
#include "lucent/content.h"
#include "lucent/log.h"
#include "zip/zip_archive.hpp"

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view headerTag = "# opensu romsets ";
constexpr std::string_view listingExtension = ".xml";
/// FinalBurn Neo's arcade listing is 14 MB, MAME 2010's 43 MB.
constexpr std::size_t maxListingBytes = std::size_t{96} * 1024 * 1024;
constexpr long httpOk = 200;
/// MAME 0.289's `-listxml` inflates to 327 MB.
constexpr std::uint32_t maxReleaseListing = 1024U * 1024U * 1024U;

std::int64_t secondsOf(RomsetDats::Clock::time_point at) {
    return std::chrono::duration_cast<std::chrono::seconds>(at.time_since_epoch()).count();
}

/// The header line `# opensu romsets <crc> <fetched>` of `text`.
std::optional<DatStamp> stampOf(std::string_view line) {
    if (!line.starts_with(headerTag)) {
        return std::nullopt;
    }
    line.remove_prefix(headerTag.size());
    const std::size_t space = line.find(' ');
    if (space == std::string_view::npos) {
        return std::nullopt;
    }
    DatStamp stamp;
    const std::string_view crc = line.substr(0, space);
    const std::string_view fetched = line.substr(space + 1);
    const auto crcEnd = std::from_chars(crc.data(), crc.data() + crc.size(), stamp.source, 16);
    const auto fetchedEnd =
        std::from_chars(fetched.data(), fetched.data() + fetched.size(), stamp.fetched);
    if (crcEnd.ec != std::errc{} || fetchedEnd.ec != std::errc{} ||
        crcEnd.ptr != crc.data() + crc.size() ||
        fetchedEnd.ptr != fetched.data() + fetched.size()) {
        return std::nullopt;
    }
    return stamp;
}

std::string hexOf(std::uint32_t value) {
    return std::format("{:08x}", value);
}

} // namespace

RomsetDats RomsetDats::under(const fs::path& cacheDir, const net::WebClient* web) {
    return RomsetDats{cacheDir / "romsets", web};
}

fs::path RomsetDats::file(const EmulatorEntry& entry) const {
    return dir_ / (entry.id + ".tsv");
}

std::optional<DatStamp> RomsetDats::stamp(const EmulatorEntry& entry) const {
    if (!enabled()) {
        return std::nullopt;
    }
    std::ifstream in{file(entry), std::ios::binary};
    std::string line;
    if (!std::getline(in, line)) {
        return std::nullopt;
    }
    return stampOf(line);
}

bool RomsetDats::refresh(const EmulatorEntry& entry, Clock::time_point now,
                         std::string& error) const {
    const std::optional<DatStamp> kept = stamp(entry);
    const auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(romsetDatLifetime);
    if (kept && secondsOf(now) - kept->fetched < lifetime.count()) {
        return true;
    }
    if (web_ == nullptr || !enabled()) {
        error = "no listing of " + entry.name + "'s romsets is kept";
        return kept.has_value();
    }
    std::optional<std::pair<RomsetDat, std::uint32_t>> fetched = fetch(entry, error);
    if (!fetched) {
        error = entry.name + "'s romset listing cannot be fetched: " + error +
                (kept ? "; the older listing stays" : "");
        return kept.has_value();
    }
    if (!save(entry, fetched->first, fetched->second, now, error)) {
        error = entry.name + "'s romset listing cannot be kept: " + error;
        return kept.has_value();
    }
    lucent::info("roms", "{}: {} romsets listed", entry.name, fetched->first.size());
    return true;
}

std::optional<std::pair<RomsetDat, std::uint32_t>> RomsetDats::fetch(const EmulatorEntry& entry,
                                                                     std::string& error) const {
    if (entry.romsets.release) {
        return fetchRelease(*entry.romsets.release, error);
    }
    RomsetDat dat;
    std::string crcs;
    for (const std::string& url : entry.romsets.files) {
        const std::string origin = net::originOf(url);
        const net::WebClient::Guard guard{
            maxListingBytes, [origin](const std::string& hop) {
                return hop.starts_with(origin) ? std::string{}
                                               : std::format("{} is not on {}", hop, origin);
            }};
        std::optional<net::WebClient::Answer> answer = web_->request(url, {}, guard, error);
        if (!answer) {
            error = std::format("{}: {}", url, error);
            return std::nullopt;
        }
        if (answer->status != httpOk) {
            error = url + ": HTTP " + std::to_string(answer->status);
            return std::nullopt;
        }
        dat.addXml(answer->body);
        crcs += hexOf(zip::crc32Of(answer->body));
    }
    return std::pair{std::move(dat), zip::crc32Of(crcs)};
}

std::optional<std::pair<RomsetDat, std::uint32_t>>
RomsetDats::fetchRelease(const ReleaseSource& release, std::string& error) const {
    const std::optional<ReleaseAsset> asset = EmulatorRelease{*web_}.latest(release, error);
    if (!asset) {
        return std::nullopt;
    }
    std::optional<std::string> bytes = web_->get(asset->url, error);
    if (!bytes) {
        error = asset->url + ": " + error;
        return std::nullopt;
    }
    if (asset->size != 0 && bytes->size() != asset->size) {
        error = std::format("{} is {} bytes, the release says {}", asset->url, bytes->size(),
                            asset->size);
        return std::nullopt;
    }
    if (!asset->sha256.empty()) {
        const std::string seen = lucent::content::sha256_hex(
            lucent::content::sha256(std::as_bytes(std::span{bytes->data(), bytes->size()})));
        if (seen != asset->sha256) {
            error = asset->url + "'s SHA-256 " + seen + " is not the release's " + asset->sha256;
            return std::nullopt;
        }
    }
    std::optional<zip::Archive> archive = zip::Archive::open(std::move(*bytes), error);
    if (!archive) {
        return std::nullopt;
    }
    const zip::Entry* listing = nullptr;
    for (const zip::Entry& entry : archive->entries()) {
        if (entry.name.ends_with(listingExtension)) {
            if (listing != nullptr) {
                error = asset->name + " holds more than one listing";
                return std::nullopt;
            }
            listing = &entry;
        }
    }
    if (listing == nullptr) {
        error = asset->name + " holds no listing";
        return std::nullopt;
    }
    const std::optional<std::string> xml = archive->read(listing->name, error, maxReleaseListing);
    if (!xml) {
        return std::nullopt;
    }
    RomsetDat dat;
    dat.addXml(*xml);
    return std::pair{std::move(dat), listing->crc32};
}

std::optional<RomsetDat> RomsetDats::load(const EmulatorEntry& entry,
                                          const RomsetDat::Wanted& wanted,
                                          std::string& error) const {
    const fs::path path = file(entry);
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        error = "no listing of " + entry.name + "'s romsets is kept";
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    const std::size_t body = text.find('\n');
    if (body == std::string::npos || !stampOf(std::string_view{text}.substr(0, body))) {
        error = path.string() + " has no header";
        return std::nullopt;
    }
    std::optional<RomsetDat> dat =
        RomsetDat::parse(std::string_view{text}.substr(body + 1), wanted, error);
    if (!dat) {
        error = path.string() + ": " + error;
    }
    return dat;
}

bool RomsetDats::save(const EmulatorEntry& entry, const RomsetDat& dat, std::uint32_t source,
                      Clock::time_point now, std::string& error) const {
    const std::string bytes =
        std::format("{}{} {}\n", headerTag, hexOf(source), secondsOf(now)) + dat.serialize();
    return fileio::writeWhole(file(entry), bytes, error);
}

} // namespace opensu::library::roms

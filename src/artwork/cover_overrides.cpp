#include "cover_overrides.hpp"

#include <fstream>
#include <iterator>
#include <system_error>
#include <variant>

#include "fileio/atomic_write.hpp"
#include "libretro_index.hpp"
#include "lucent/log.h"

namespace opensu::artwork {
namespace {

namespace fs = std::filesystem;

bool isFile(const fs::path& file) {
    std::error_code ec;
    return fs::is_regular_file(file, ec);
}

fs::path withSuffix(fs::path file, std::string_view suffix) {
    file += std::string{suffix};
    return file;
}

} // namespace

CoverOverrides::CoverOverrides(fs::path chosenDir, fs::path fittedDir)
    : chosenDir_{std::move(chosenDir)}, fittedDir_{std::move(fittedDir)} {
}

fs::path CoverOverrides::chosenPath(std::string_view id) const {
    return chosenDir_ / encodeSegment(id);
}

fs::path CoverOverrides::ownMarker(std::string_view id) const {
    return withSuffix(chosenPath(id), ".own");
}

fs::path CoverOverrides::fittedPath(std::string_view id) const {
    return fittedDir_ / encodeSegment(id);
}

fs::path CoverOverrides::shown(std::string_view id) const {
    if (const fs::path chosen = chosenPath(id); isFile(chosen)) {
        return chosen;
    }
    if (const fs::path fitted = fittedPath(id); !keptOwn(id) && isFile(fitted)) {
        return fitted;
    }
    return {};
}

bool CoverOverrides::keptOwn(std::string_view id) const {
    return isFile(ownMarker(id));
}

bool CoverOverrides::wantsFit(std::string_view id, ArtworkStore::Clock::time_point now) const {
    if (isFile(chosenPath(id)) || keptOwn(id) || isFile(fittedPath(id))) {
        return false;
    }
    std::error_code ec;
    const fs::file_time_type missed = fs::last_write_time(withSuffix(fittedPath(id), ".miss"), ec);
    return ec || now - missed >= ArtworkStore::missLifetime;
}

void CoverOverrides::apply(std::vector<library::ShelfItem>& shelf) const {
    for (library::ShelfItem& item : shelf) {
        if (auto* game = std::get_if<library::Game>(&item)) {
            if (fs::path cover = shown(game->id); !cover.empty()) {
                game->artwork = std::move(cover);
            }
        }
    }
}

bool CoverOverrides::choose(std::string_view id, std::string_view bytes, std::string& error) const {
    if (!fileio::writeWhole(chosenPath(id), bytes, error)) {
        return false;
    }
    std::error_code ec;
    fs::remove(ownMarker(id), ec);
    return true;
}

bool CoverOverrides::chooseOwn(std::string_view id, std::string& error) const {
    std::error_code ec;
    fs::remove(chosenPath(id), ec);
    return fileio::writeWhole(ownMarker(id), {}, error);
}

bool CoverOverrides::keepLookup(std::string_view id, std::string_view lookup,
                                std::string& error) const {
    return fileio::writeWhole(withSuffix(fittedPath(id), ".for"), lookup, error);
}

bool CoverOverrides::fit(std::string_view id, std::string_view bytes, std::string_view lookup,
                         std::string& error) const {
    return keepLookup(id, lookup, error) && fileio::writeWhole(fittedPath(id), bytes, error);
}

void CoverOverrides::recordFitMiss(std::string_view id, std::string_view lookup) const {
    std::string error;
    if (!keepLookup(id, lookup, error) ||
        !fileio::writeWhole(withSuffix(fittedPath(id), ".miss"), {}, error)) {
        lucent::warn("artwork", "{}", error);
    }
}

void CoverOverrides::forgetOtherFit(std::string_view id, std::string_view lookup) const {
    const fs::path fitted = fittedPath(id);
    const fs::path miss = withSuffix(fitted, ".miss");
    if (!isFile(fitted) && !isFile(miss)) {
        return;
    }
    const fs::path kept = withSuffix(fitted, ".for");
    std::ifstream in{kept, std::ios::binary};
    if (in && std::string{std::istreambuf_iterator<char>{in}, {}} == lookup) {
        return;
    }
    std::error_code ec;
    fs::remove(fitted, ec);
    fs::remove(miss, ec);
    fs::remove(kept, ec);
}

} // namespace opensu::artwork

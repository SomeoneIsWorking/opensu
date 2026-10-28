#include "console_art.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "gpu2d/image/decode.hpp"

namespace opensu::ui {
namespace {

using Read = std::optional<std::string> (artwork::IisuPack::*)(std::string_view,
                                                               std::string&) const;

/// A system's card or glyph from `pack`, decoded on a loader worker.
gpu2d::ImageHandle load(gpu2d::ImageLoader& images,
                        const std::shared_ptr<const artwork::IisuPack>& pack,
                        const std::string& system, Read read, std::string_view what) {
    return images.load(gpu2d::ImageRequest{
        .key = pack->file().string() + " " + std::string{what} + " " + system,
        .decode = [pack, system, read, what = std::string{what}](
                      std::string& error) -> std::optional<gpu2d::image::Bitmap> {
            const std::optional<std::string> bytes = ((*pack).*read)(system, error);
            if (!bytes) {
                if (error.empty()) {
                    error = "the iiSU file has no " + what + " for " + system;
                }
                return std::nullopt;
            }
            return gpu2d::image::decode(std::as_bytes(std::span{*bytes}), error);
        },
        // Both are drawn smaller than their files, so mipmaps keep them clean.
        .filter = gpu2d::Filter::Trilinear});
}

} // namespace

void ConsoleArt::useIisu(std::shared_ptr<const artwork::IisuPack> pack,
                         std::vector<std::string> systems) {
    if (pack == nullptr || pack_ == nullptr || pack->file() != pack_->file()) {
        entries_.clear();
    }
    pack_ = std::move(pack);
    systems_ = std::move(systems);
}

void ConsoleArt::request() {
    if (pack_ == nullptr) {
        return;
    }
    for (const std::string& system : systems_) {
        Entry& entry = entries_[system];
        if (entry.card.state() == gpu2d::ImageState::Empty) {
            entry.card = load(images_, pack_, system, &artwork::IisuPack::card, "card");
        }
        if (pack_->hasGlyphs() && entry.glyph.state() == gpu2d::ImageState::Empty) {
            entry.glyph = load(images_, pack_, system, &artwork::IisuPack::glyph, "glyph");
        }
    }
}

std::size_t ConsoleArt::pending() const noexcept {
    if (pack_ == nullptr) {
        return 0;
    }
    return static_cast<std::size_t>(std::ranges::count_if(entries_, [](const auto& pair) {
        return pair.second.card.pending() || pair.second.glyph.pending();
    }));
}

const ConsoleArt::Entry* ConsoleArt::find(std::string_view system) const {
    if (pack_ == nullptr) {
        return nullptr;
    }
    const auto found = entries_.find(system);
    return found == entries_.end() ? nullptr : &found->second;
}

const gpu2d::Texture* ConsoleArt::iisuCard(std::string_view system) const {
    const Entry* entry = find(system);
    return entry == nullptr ? nullptr : entry->card.texture();
}

const gpu2d::Texture* ConsoleArt::drawing(std::string_view system, int pixels) {
    return icons_.mask(consoleCardFile(system), pixels);
}

const gpu2d::Texture* ConsoleArt::glyph(std::string_view system, int pixels) {
    if (const Entry* entry = find(system); entry != nullptr && entry->glyph.texture() != nullptr) {
        return entry->glyph.texture();
    }
    return icons_.mask(consoleGlyphFile(system), pixels);
}

} // namespace opensu::ui

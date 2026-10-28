#include "console_art.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>

#include "gpu2d/image/decode.hpp"

namespace opensu::ui {
namespace {

using artwork::ThemeArt;

/// A theme picture, decoded on a loader worker.
gpu2d::ImageHandle load(gpu2d::ImageLoader& images, const ThemeArt& art) {
    return images.load(gpu2d::ImageRequest{
        .key = art.key,
        .decode = [file = art.file,
                   bytes = art.bytes](std::string& error) -> std::optional<gpu2d::image::Bitmap> {
            if (!bytes) {
                return gpu2d::image::decodeFile(file, error);
            }
            const std::optional<std::string> encoded = bytes(error);
            if (!encoded) {
                return std::nullopt;
            }
            return gpu2d::image::decode(std::as_bytes(std::span{*encoded}), error);
        },
        // Pictures are drawn smaller than their files, so mipmaps keep them clean.
        .filter = gpu2d::Filter::Trilinear});
}

} // namespace

void ConsoleArt::use(std::shared_ptr<const artwork::ConsoleTheme> theme,
                     const std::vector<std::string>& systems) {
    if (theme != theme_) {
        entries_.clear();
    }
    theme_ = std::move(theme);
    if (theme_ == nullptr) {
        return;
    }
    for (const std::string& system : systems) {
        if (!entries_.contains(system)) {
            entries_.emplace(system, Entry{.card = theme_->card(system),
                                           .glyph = theme_->glyph(system),
                                           .cardImage = {},
                                           .glyphImage = {}});
        }
    }
}

void ConsoleArt::request() {
    for (auto& [system, entry] : entries_) {
        const std::optional<ThemeArt>& card = entry.card;
        if (card && card->kind == ThemeArt::Kind::Picture &&
            entry.cardImage.state() == gpu2d::ImageState::Empty) {
            entry.cardImage = load(images_, *card);
        }
        const std::optional<ThemeArt>& glyph = entry.glyph;
        if (glyph && glyph->kind == ThemeArt::Kind::Picture &&
            entry.glyphImage.state() == gpu2d::ImageState::Empty) {
            entry.glyphImage = load(images_, *glyph);
        }
    }
}

std::size_t ConsoleArt::pending() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(entries_, [](const auto& pair) {
        return pair.second.cardImage.pending() || pair.second.glyphImage.pending();
    }));
}

const ConsoleArt::Entry* ConsoleArt::find(std::string_view system) const {
    const auto found = entries_.find(system);
    return found == entries_.end() ? nullptr : &found->second;
}

CardLook ConsoleArt::look(std::string_view system) const {
    const Entry* entry = find(system);
    if (entry == nullptr || !entry->card) {
        return CardLook::None;
    }
    if (entry->card->kind == ThemeArt::Kind::Mask) {
        return CardLook::Mask;
    }
    // A picture that could not be read is no picture.
    if (entry->cardImage.state() == gpu2d::ImageState::Failed) {
        return CardLook::None;
    }
    return theme_->cardStyle() == artwork::CardStyle::Tile ? CardLook::Tile : CardLook::Picture;
}

const gpu2d::Texture* ConsoleArt::texture(const std::optional<ThemeArt>& art,
                                          const gpu2d::ImageHandle& image, int pixels) {
    if (!art) {
        return nullptr;
    }
    if (art->kind == ThemeArt::Kind::Mask) {
        return icons_.mask(art->file.string(), pixels);
    }
    return image.texture();
}

const gpu2d::Texture* ConsoleArt::card(std::string_view system, int pixels) {
    const Entry* entry = find(system);
    return entry == nullptr ? nullptr : texture(entry->card, entry->cardImage, pixels);
}

const gpu2d::Texture* ConsoleArt::glyph(std::string_view system, int pixels) {
    const Entry* entry = find(system);
    return entry == nullptr ? nullptr : texture(entry->glyph, entry->glyphImage, pixels);
}

} // namespace opensu::ui

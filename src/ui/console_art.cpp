#include "console_art.hpp"

#include <algorithm>
#include <string>

#include "gpu2d/image/decode.hpp"

#include "lucent/log.h"

namespace opensu::ui {

void ConsoleArt::useIisu(bool iisu) noexcept {
    iisu_ = iisu;
}

void ConsoleArt::setIisuCard(std::string_view system, const std::filesystem::path& file) {
    record(cards_, system, file);
}

void ConsoleArt::setIisuGlyph(std::string_view system, const std::filesystem::path& file) {
    record(glyphs_, system, file);
}

void ConsoleArt::record(Entries& entries, std::string_view system,
                        const std::filesystem::path& file) {
    Entry& entry = entries[std::string{system}];
    if (entry.file == file && entry.loaded) {
        return;
    }
    entry.texture.reset();
    entry.loaded = false;
    entry.file = file;
}

void ConsoleArt::load(SDL_GPUDevice& device) {
    if (!iisu_) {
        return;
    }
    load(cards_, device);
    load(glyphs_, device);
}

void ConsoleArt::load(Entries& entries, SDL_GPUDevice& device) {
    for (auto& [system, entry] : entries) {
        if (entry.loaded) {
            continue;
        }
        entry.loaded = true;
        std::string error;
        const std::optional<gpu2d::image::Bitmap> pixels =
            gpu2d::image::decodeFile(entry.file, error);
        if (!pixels) {
            lucent::warn("ui", "iiSU console art {} unreadable: {}", entry.file.string(), error);
            continue;
        }
        // Both are drawn smaller than their files, so mipmaps keep them clean.
        entry.texture.emplace(device, pixels->rgba, pixels->width, pixels->height,
                              gpu2d::Filter::Trilinear);
    }
}

std::size_t ConsoleArt::pending() const noexcept {
    if (!iisu_) {
        return 0;
    }
    const auto waiting = [](const Entries& entries) {
        return static_cast<std::size_t>(std::ranges::count_if(entries, [](const auto& pair) {
            return !pair.second.loaded;
        }));
    };
    return waiting(cards_) + waiting(glyphs_);
}

const gpu2d::Texture* ConsoleArt::find(const Entries& entries, std::string_view system) const {
    if (!iisu_) {
        return nullptr;
    }
    const auto found = entries.find(system);
    return found != entries.end() && found->second.texture ? &*found->second.texture : nullptr;
}

const gpu2d::Texture* ConsoleArt::iisuCard(std::string_view system) const {
    return find(cards_, system);
}

const gpu2d::Texture* ConsoleArt::drawing(std::string_view system, int pixels) {
    return icons_.mask(consoleCardFile(system), pixels);
}

const gpu2d::Texture* ConsoleArt::glyph(std::string_view system, int pixels) {
    if (const gpu2d::Texture* iisu = find(glyphs_, system)) {
        return iisu;
    }
    return icons_.mask(consoleGlyphFile(system), pixels);
}

} // namespace opensu::ui

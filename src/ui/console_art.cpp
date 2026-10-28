#include "console_art.hpp"

#include <algorithm>
#include <string>

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
    if (entry.file != file) {
        entry.file = file;
        entry.image = {};
    }
}

void ConsoleArt::request() {
    if (!iisu_) {
        return;
    }
    request(cards_);
    request(glyphs_);
}

void ConsoleArt::request(Entries& entries) {
    for (auto& [system, entry] : entries) {
        if (entry.image.state() == gpu2d::ImageState::Empty) {
            // Both are drawn smaller than their files, so mipmaps keep them clean.
            entry.image = images_.loadFile(entry.file, gpu2d::Filter::Trilinear);
        }
    }
}

std::size_t ConsoleArt::pending() const noexcept {
    if (!iisu_) {
        return 0;
    }
    const auto waiting = [](const Entries& entries) {
        return static_cast<std::size_t>(std::ranges::count_if(entries, [](const auto& pair) {
            return pair.second.image.pending();
        }));
    };
    return waiting(cards_) + waiting(glyphs_);
}

const gpu2d::Texture* ConsoleArt::find(const Entries& entries, std::string_view system) const {
    if (!iisu_) {
        return nullptr;
    }
    const auto found = entries.find(system);
    return found != entries.end() ? found->second.image.texture() : nullptr;
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

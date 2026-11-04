#include "ps1_cards.hpp"

#include <system_error>

#include "ini_file.hpp"

namespace opensu::library::saves {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view section = "MemoryCards";
constexpr std::string_view perGameTitle = "PerGameTitle";
constexpr std::string_view perGameFileTitle = "PerGameFileTitle";
constexpr unsigned char lastControl = 31;

/// `name` as DuckStation's `Path::SanitizeFileName` makes it a file name on Linux: a control
/// character, `*` or `/` made `_`.
std::string sanitized(std::string_view name) {
    std::string sane{name};
    for (char& c : sane) {
        if (static_cast<unsigned char>(c) <= lastControl || c == '*' || c == '/') {
            c = '_';
        }
    }
    return sane;
}

fs::path cardNamed(const fs::path& folder, std::string_view title) {
    return folder / (sanitized(title) + "_1.mcd");
}

} // namespace

std::optional<fs::path> duckStationCard(const fs::path& data, const fs::path& folder,
                                        std::string_view title, const fs::path& rom) {
    const fs::path settings = data / "settings.ini";
    const std::string type =
        iniValue(settings, section, "Card1Type").value_or(std::string{perGameTitle});
    const fs::path named = iniValue(settings, section, "Directory").value_or("");
    const fs::path cards = named.empty() ? folder : named.is_absolute() ? named : data / named;
    fs::path byFile = cardNamed(cards, rom.stem().string());
    if (type == perGameFileTitle) {
        return byFile;
    }
    if (type != perGameTitle) {
        return std::nullopt;
    }
    // A disc its database knows is named by its title there, which openSU's title stands for;
    // one it does not know, by its file.
    const fs::path byTitle = cardNamed(cards, title);
    std::error_code ec;
    return fs::exists(byTitle, ec) || !fs::exists(byFile, ec) ? byTitle : byFile;
}

std::optional<std::string> wholeCard(std::string card, std::string& error) {
    if (card.size() != ps1CardBytes) {
        error = "a memory card is " + std::to_string(ps1CardBytes) + " bytes, this is " +
                std::to_string(card.size());
        return std::nullopt;
    }
    return card;
}

} // namespace opensu::library::saves

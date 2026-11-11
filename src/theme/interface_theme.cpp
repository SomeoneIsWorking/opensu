#include "interface_theme.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <initializer_list>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

namespace opensu::theme {
namespace {

using Json = nlohmann::json;

/// The format's version this reader knows.
constexpr int formatVersion = 1;
constexpr std::size_t nameLimit = 48;
constexpr std::size_t lineLimit = 160;
constexpr std::size_t pathLimit = 128;
/// Bounds for type sizes and lengths, in dp.
constexpr float typeLeast = 6.0f;
constexpr float typeMost = 64.0f;
constexpr float lengthMost = 1200.0f;
constexpr float growMost = 1.3f;
constexpr float aspectMost = 2.0f;
constexpr int pageColumnsMost = 8;
constexpr int pageRowsMost = 6;

constexpr std::array faceExtensions{".ttf", ".otf"};
constexpr std::array soundExtensions{".ogg", ".wav"};

/// What a theme's sound set may be: openSU's shipped sets, never iiSU's.
constexpr std::array shippedSets{settings::SoundSet::Marimba, settings::SoundSet::Glass,
                                 settings::SoundSet::Pluck};

/// `relative` under `folder` with links followed, when it stays inside `folder`.
std::optional<std::filesystem::path> within(const std::filesystem::path& folder,
                                            const std::filesystem::path& relative) {
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::weakly_canonical(folder, ec);
    if (ec) {
        return std::nullopt;
    }
    std::filesystem::path resolved = std::filesystem::weakly_canonical(folder / relative, ec);
    const auto [rootEnd, at] = std::ranges::mismatch(root, resolved);
    if (ec || rootEnd != root.end()) {
        return std::nullopt;
    }
    return resolved;
}

/// Reads one object of the manifest, naming each value by its path for an error.
class Reader {
  public:
    Reader(const Json& object, std::string where, const std::filesystem::path& folder,
           std::string& error)
        : object_{object}, where_{std::move(where)}, folder_{folder}, error_{error} {
    }

    /// False, with the error set, when the object holds a key not in `known`.
    bool only(std::initializer_list<std::string_view> known) {
        for (const auto& [key, value] : object_.items()) {
            if (std::ranges::find(known, key) == known.end()) {
                return fail(name(key) + " is not a key the format knows");
            }
        }
        return true;
    }

    bool text(std::string_view key, std::string& out, std::size_t limit) {
        const Json* value = find(key);
        if (value == nullptr) {
            return true;
        }
        if (!value->is_string()) {
            return fail(name(key) + " is not a string");
        }
        const auto& text = value->get_ref<const std::string&>();
        if (text.size() > limit) {
            return fail(name(key) + " is longer than " + std::to_string(limit) + " bytes");
        }
        if (std::ranges::any_of(text, [](char c) {
                return static_cast<unsigned char>(c) < 0x20;
            })) {
            return fail(name(key) + " holds a control character");
        }
        out = text;
        return true;
    }

    bool flag(std::string_view key, bool& out) {
        const Json* value = find(key);
        if (value == nullptr) {
            return true;
        }
        if (!value->is_boolean()) {
            return fail(name(key) + " is not true or false");
        }
        out = value->get<bool>();
        return true;
    }

    bool number(std::string_view key, float& out, float least, float most) {
        const Json* value = find(key);
        if (value == nullptr) {
            return true;
        }
        if (!value->is_number()) {
            return fail(name(key) + " is not a number");
        }
        const double read = value->get<double>();
        if (!(read >= least && read <= most)) {
            return fail(name(key) + " is outside " + std::to_string(least) + " to " +
                        std::to_string(most));
        }
        out = static_cast<float>(read);
        return true;
    }

    bool whole(std::string_view key, int& out, int least, int most) {
        const Json* value = find(key);
        if (value == nullptr) {
            return true;
        }
        if (!value->is_number_integer()) {
            return fail(name(key) + " is not a whole number");
        }
        const auto read = value->get<std::int64_t>();
        if (read < least || read > most) {
            return fail(name(key) + " is outside " + std::to_string(least) + " to " +
                        std::to_string(most));
        }
        out = static_cast<int>(read);
        return true;
    }

    bool colour(std::string_view key, Colour& out) {
        const Json* value = find(key);
        if (value == nullptr) {
            return true;
        }
        const std::optional<Colour> read =
            value->is_string() ? parsedColour(value->get_ref<const std::string&>()) : std::nullopt;
        if (!read) {
            return fail(name(key) + " is not a colour, #RRGGBB or #RRGGBBAA");
        }
        out = *read;
        return true;
    }

    /// A file of the folder: relative, inside it once links are followed, with one of
    /// `extensions` and at most `limit` bytes.
    template <std::size_t N>
    bool file(std::string_view key, std::filesystem::path& out,
              const std::array<const char*, N>& extensions, std::uintmax_t limit) {
        std::optional<std::filesystem::path> path;
        if (!inside(key, path)) {
            return false;
        }
        if (!path) {
            return true;
        }
        const std::string extension = path->extension().string();
        if (std::ranges::none_of(extensions, [&extension](const char* known) {
                return extension == known;
            })) {
            return fail(name(key) + " is not a " + joined(extensions) + " file");
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(*path, ec)) {
            return fail(name(key) + " names no file in the theme's folder");
        }
        if (std::filesystem::file_size(*path, ec) > limit || ec) {
            return fail(name(key) + " is larger than " + std::to_string(limit >> 20U) + " MiB");
        }
        out = *path;
        return true;
    }

    /// A folder of the folder.
    bool directory(std::string_view key, std::filesystem::path& out) {
        std::optional<std::filesystem::path> path;
        if (!inside(key, path)) {
            return false;
        }
        if (!path) {
            return true;
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(*path, ec)) {
            return fail(name(key) + " names no folder in the theme's folder");
        }
        out = *path;
        return true;
    }

    /// The nested object at `key`, or none; false, with the error set, for another type.
    bool object(std::string_view key, const Json*& out) {
        out = find(key);
        if (out != nullptr && !out->is_object()) {
            return fail(name(key) + " is not an object");
        }
        return true;
    }

    [[nodiscard]] const Json* find(std::string_view key) const {
        const auto found = object_.find(key);
        return found == object_.end() ? nullptr : &*found;
    }

    [[nodiscard]] std::string name(std::string_view key) const {
        return where_.empty() ? std::string{key} : where_ + "." + std::string{key};
    }

    bool fail(std::string why) {
        error_ = std::move(why);
        return false;
    }

  private:
    static std::optional<Colour> parsedColour(std::string_view text) {
        if ((text.size() != 7 && text.size() != 9) || text.front() != '#') {
            return std::nullopt;
        }
        std::array<std::uint8_t, 4> parts{0, 0, 0, 255};
        for (std::size_t i = 0; i * 2 + 1 < text.size(); ++i) {
            const char* begin = text.data() + 1 + i * 2;
            const auto [end, ec] = std::from_chars(begin, begin + 2, parts.at(i), 16);
            if (ec != std::errc{} || end != begin + 2) {
                return std::nullopt;
            }
        }
        return Colour{parts[0], parts[1], parts[2], parts[3]};
    }

    template <std::size_t N> static std::string joined(const std::array<const char*, N>& list) {
        std::string out;
        for (const char* item : list) {
            out += out.empty() ? item : std::string{" or "} + item;
        }
        return out;
    }

    /// The path at `key` within the folder; nothing when unset.
    bool inside(std::string_view key, std::optional<std::filesystem::path>& out) {
        std::string spelled;
        if (!text(key, spelled, pathLimit)) {
            return false;
        }
        if (find(key) == nullptr) {
            return true;
        }
        const std::filesystem::path relative{spelled};
        const bool climbs = std::ranges::any_of(relative, [](const std::filesystem::path& part) {
            return part == "..";
        });
        if (spelled.empty() || relative.has_root_path() || climbs ||
            spelled.find('\\') != std::string::npos) {
            return fail(name(key) + " is not a path inside the theme's folder");
        }
        out = within(folder_, relative);
        if (!out) {
            return fail(name(key) + " leads out of the theme's folder");
        }
        return true;
    }

    const Json& object_;
    std::string where_;
    const std::filesystem::path& folder_;
    std::string& error_;
};

bool readPalette(Reader& reader, Palette& palette) {
    return reader.only({"ground",   "surface",   "control",   "controlFocused", "selected",
                        "ink",      "inkMuted",  "inkFaint",  "accent",         "onAccent",
                        "track",    "focusRing", "scrim",     "toast",          "toastError",
                        "onToast",  "danger",    "groundTop", "groundBottom",   "groundDots",
                        "chromeInk"}) &&
           reader.colour("ground", palette.ground) && reader.colour("surface", palette.surface) &&
           reader.colour("control", palette.control) &&
           reader.colour("controlFocused", palette.controlFocused) &&
           reader.colour("selected", palette.selected) && reader.colour("ink", palette.ink) &&
           reader.colour("inkMuted", palette.inkMuted) &&
           reader.colour("inkFaint", palette.inkFaint) && reader.colour("accent", palette.accent) &&
           reader.colour("onAccent", palette.onAccent) && reader.colour("track", palette.track) &&
           reader.colour("focusRing", palette.focusRing) && reader.colour("scrim", palette.scrim) &&
           reader.colour("toast", palette.toast) &&
           reader.colour("toastError", palette.toastError) &&
           reader.colour("onToast", palette.onToast) && reader.colour("danger", palette.danger) &&
           reader.colour("groundTop", palette.groundTop) &&
           reader.colour("groundBottom", palette.groundBottom) &&
           reader.flag("groundDots", palette.groundDots) &&
           reader.colour("chromeInk", palette.chromeInk);
}

bool readType(Reader& reader, Type& type) {
    return reader.only({"face", "title", "heading", "body", "caption", "value", "hint", "toast"}) &&
           reader.file("face", type.face, faceExtensions, faceLimit) &&
           reader.number("title", type.title, typeLeast, typeMost) &&
           reader.number("heading", type.heading, typeLeast, typeMost) &&
           reader.number("body", type.body, typeLeast, typeMost) &&
           reader.number("caption", type.caption, typeLeast, typeMost) &&
           reader.number("value", type.value, typeLeast, typeMost) &&
           reader.number("hint", type.hint, typeLeast, typeMost) &&
           reader.number("toast", type.toast, typeLeast, typeMost);
}

bool readShape(Reader& reader, Shape& shape) {
    constexpr float most = 40.0f;
    return reader.only({"radius", "smallRadius", "outline", "focusRing", "focusGap"}) &&
           reader.number("radius", shape.radius, 0.0f, most) &&
           reader.number("smallRadius", shape.smallRadius, 0.0f, most) &&
           reader.number("outline", shape.outline, 0.0f, 8.0f) &&
           reader.number("focusRing", shape.focusRing, 0.0f, 8.0f) &&
           reader.number("focusGap", shape.focusGap, 0.0f, 8.0f);
}

bool readSpacing(Reader& reader, Spacing& spacing) {
    return reader.only({"gap", "pad", "section", "rowHeight", "compactRowHeight", "sidebarWidth",
                        "contentWidth", "hintBarHeight", "dialogWidth"}) &&
           reader.number("gap", spacing.gap, 0.0f, 64.0f) &&
           reader.number("pad", spacing.pad, 0.0f, 64.0f) &&
           reader.number("section", spacing.section, 0.0f, 64.0f) &&
           reader.number("rowHeight", spacing.rowHeight, 24.0f, 160.0f) &&
           reader.number("compactRowHeight", spacing.compactRowHeight, 20.0f, 160.0f) &&
           reader.number("sidebarWidth", spacing.sidebarWidth, 80.0f, lengthMost) &&
           reader.number("contentWidth", spacing.contentWidth, 200.0f, lengthMost) &&
           reader.number("hintBarHeight", spacing.hintBarHeight, 20.0f, 120.0f) &&
           reader.number("dialogWidth", spacing.dialogWidth, 200.0f, lengthMost);
}

bool readTiles(Reader& reader, Tiles& tiles) {
    if (!reader.only({"style", "grow", "body", "edge", "well", "focus", "aspect"})) {
        return false;
    }
    std::string style;
    if (!reader.text("style", style, nameLimit)) {
        return false;
    }
    if (reader.find("style") != nullptr) {
        if (style == "glass") {
            tiles.style = TileStyle::Glass;
        } else if (style == "channel") {
            tiles.style = TileStyle::Channel;
        } else {
            return reader.fail(reader.name("style") + " is not glass or channel");
        }
    }
    return reader.number("grow", tiles.grow, 1.0f, growMost) && reader.colour("body", tiles.body) &&
           reader.colour("edge", tiles.edge) && reader.colour("well", tiles.well) &&
           reader.colour("focus", tiles.focus) &&
           reader.number("aspect", tiles.aspect, 1.0f, aspectMost);
}

bool readHome(Reader& reader, Home& home) {
    if (!reader.only({"scroll", "clockBar", "pillClock", "columns", "rows", "fill"})) {
        return false;
    }
    std::string scroll;
    if (!reader.text("scroll", scroll, nameLimit)) {
        return false;
    }
    if (reader.find("scroll") != nullptr) {
        if (scroll != "pages" && scroll != "scrolling") {
            return reader.fail(reader.name("scroll") + " is not pages or scrolling");
        }
        home.pages = scroll == "pages";
    }
    std::string fill;
    if (!reader.text("fill", fill, nameLimit)) {
        return false;
    }
    if (reader.find("fill") != nullptr) {
        if (fill != "rows" && fill != "columns") {
            return reader.fail(reader.name("fill") + " is not rows or columns");
        }
        home.fillRows = fill == "rows";
    }
    return reader.flag("clockBar", home.clockBar) && reader.flag("pillClock", home.pillClock) &&
           reader.whole("columns", home.columns, 1, pageColumnsMost) &&
           reader.whole("rows", home.rows, 1, pageRowsMost);
}

bool readSounds(Reader& reader, Sounds& sounds) {
    if (!reader.only({"set", "folder"})) {
        return false;
    }
    std::string set;
    if (!reader.text("set", set, nameLimit)) {
        return false;
    }
    if (reader.find("set") != nullptr) {
        const std::optional<settings::SoundSet> named = settings::soundSetOf(set);
        if (!named || std::ranges::find(shippedSets, *named) == shippedSets.end()) {
            return reader.fail(reader.name("set") + " is not marimba, glass or pluck");
        }
        sounds.set = *named;
    }
    return reader.directory("folder", sounds.folder);
}

/// Reads the nested object `key` with `read`, when there is one.
template <typename Part>
bool nested(Reader& top, std::string_view key, const std::filesystem::path& folder,
            std::string& error, Part& part, bool (*read)(Reader&, Part&)) {
    const Json* object = nullptr;
    if (!top.object(key, object)) {
        return false;
    }
    if (object == nullptr) {
        return true;
    }
    Reader reader{*object, std::string{key}, folder, error};
    return read(reader, part);
}

} // namespace

std::optional<InterfaceTheme> parseTheme(std::string_view manifest,
                                         const std::filesystem::path& folder, std::string id,
                                         std::string& error) {
    if (manifest.size() > manifestLimit) {
        error = std::string{manifestFile} + " is larger than " +
                std::to_string(manifestLimit >> 10U) + " KiB";
        return std::nullopt;
    }
    const Json document = Json::parse(manifest, nullptr, false);
    if (!document.is_object()) {
        error = std::string{manifestFile} + " is not a JSON object";
        return std::nullopt;
    }
    InterfaceTheme theme;
    theme.id = std::move(id);
    theme.folder = folder;
    Reader top{document, {}, folder, error};
    if (!top.only({"format", "name", "author", "note", "palette", "type", "shape", "spacing",
                   "tiles", "home", "sounds", "music"})) {
        return std::nullopt;
    }
    const Json* format = top.find("format");
    if (format == nullptr || !format->is_number_integer() || format->get<int>() != formatVersion) {
        error = "format is not " + std::to_string(formatVersion);
        return std::nullopt;
    }
    if (!top.text("name", theme.name, nameLimit) || !top.text("author", theme.author, lineLimit) ||
        !top.text("note", theme.note, lineLimit)) {
        return std::nullopt;
    }
    if (theme.name.empty()) {
        error = "name is missing";
        return std::nullopt;
    }
    const bool read = nested(top, "palette", folder, error, theme.palette, readPalette) &&
                      nested(top, "type", folder, error, theme.type, readType) &&
                      nested(top, "shape", folder, error, theme.shape, readShape) &&
                      nested(top, "spacing", folder, error, theme.spacing, readSpacing) &&
                      nested(top, "tiles", folder, error, theme.tiles, readTiles) &&
                      nested(top, "home", folder, error, theme.home, readHome) &&
                      nested(top, "sounds", folder, error, theme.sounds, readSounds) &&
                      top.file("music", theme.music, soundExtensions, musicLimit);
    if (!read) {
        return std::nullopt;
    }
    return theme;
}

std::optional<InterfaceTheme> readTheme(const std::filesystem::path& folder, std::string id,
                                        std::string& error) {
    const std::filesystem::path file = folder / manifestFile;
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = file.string() + " is missing";
        return std::nullopt;
    }
    std::string manifest;
    manifest.resize(manifestLimit + 1);
    in.read(manifest.data(), static_cast<std::streamsize>(manifest.size()));
    manifest.resize(static_cast<std::size_t>(in.gcount()));
    std::optional<InterfaceTheme> theme = parseTheme(manifest, folder, std::move(id), error);
    if (!theme) {
        error = file.string() + ": " + error;
    }
    return theme;
}

std::optional<std::filesystem::path> soundFile(const InterfaceTheme& theme, std::string_view name,
                                               std::string& error) {
    error.clear();
    std::error_code ec;
    if (theme.sounds.folder.empty() ||
        !std::filesystem::exists(std::filesystem::symlink_status(theme.sounds.folder / name, ec))) {
        return std::nullopt;
    }
    std::optional<std::filesystem::path> file =
        within(theme.sounds.folder, std::filesystem::path{name});
    if (!file) {
        error = "leads out of the theme's folder";
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(*file, ec)) {
        error = "is not a file";
        return std::nullopt;
    }
    if (std::filesystem::file_size(*file, ec) > soundLimit || ec) {
        error = "is larger than " + std::to_string(soundLimit >> 20U) + " MiB";
        return std::nullopt;
    }
    return file;
}

} // namespace opensu::theme

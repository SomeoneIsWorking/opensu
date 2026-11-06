#include "romset_dat.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>

namespace opensu::library::roms {
namespace {

constexpr char field = '\t';
/// Parent and BIOS links followed from a set, past any a listing could hold.
constexpr std::size_t maxChain = 8;

constexpr std::array<std::pair<std::string_view, char>, 5> entities{
    {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}}};

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// `text` with the five XML entities turned back into their characters.
std::string unescaped(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    while (!text.empty()) {
        const std::size_t amp = text.find('&');
        out.append(text.substr(0, amp));
        if (amp == std::string_view::npos) {
            break;
        }
        text.remove_prefix(amp);
        const auto entity = std::ranges::find_if(entities, [text](const auto& known) {
            return text.starts_with(known.first);
        });
        if (entity == entities.end()) {
            out.push_back('&');
            text.remove_prefix(1);
        } else {
            out.push_back(entity->second);
            text.remove_prefix(entity->first.size());
        }
    }
    return out;
}

/// One start or end tag: its element name and the attribute text after it.
struct Tag {
    std::string_view element;
    std::string_view attributes;
    bool closing{false};
    bool empty{false};
};

/// The value of attribute `key` in a tag's attribute text; nothing when it has none.
std::optional<std::string> attribute(std::string_view attributes, std::string_view key) {
    std::size_t at = 0;
    while ((at = attributes.find(key, at)) != std::string_view::npos) {
        const bool starts =
            at == 0 || std::isspace(static_cast<unsigned char>(attributes[at - 1])) != 0;
        const std::size_t equals = at + key.size();
        if (starts && equals + 1 < attributes.size() && attributes[equals] == '=' &&
            (attributes[equals + 1] == '"' || attributes[equals + 1] == '\'')) {
            const char quote = attributes[equals + 1];
            const std::size_t end = attributes.find(quote, equals + 2);
            if (end == std::string_view::npos) {
                return std::nullopt;
            }
            return unescaped(attributes.substr(equals + 2, end - equals - 2));
        }
        at = equals;
    }
    return std::nullopt;
}

std::string attributeOr(std::string_view attributes, std::string_view key) {
    return attribute(attributes, key).value_or(std::string{});
}

template <class Number> std::optional<Number> numberOf(std::string_view text, int base) {
    Number value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (text.empty() || ec != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// The next tag at or after `at` in `xml`, with `at` moved past it; nothing at the end.
std::optional<Tag> nextTag(std::string_view xml, std::size_t& at) {
    while (true) {
        const std::size_t open = xml.find('<', at);
        if (open == std::string_view::npos) {
            at = xml.size();
            return std::nullopt;
        }
        if (xml.substr(open, 4) == "<!--") {
            const std::size_t end = xml.find("-->", open);
            at = end == std::string_view::npos ? xml.size() : end + 3;
            continue;
        }
        const std::size_t close = xml.find('>', open);
        if (close == std::string_view::npos) {
            at = xml.size();
            return std::nullopt;
        }
        at = close + 1;
        std::string_view body = xml.substr(open + 1, close - open - 1);
        if (body.empty() || body.front() == '!' || body.front() == '?') {
            continue;
        }
        Tag tag;
        if (body.front() == '/') {
            tag.closing = true;
            body.remove_prefix(1);
        }
        if (body.ends_with('/')) {
            tag.empty = true;
            body.remove_suffix(1);
        }
        std::size_t nameEnd = 0;
        while (nameEnd < body.size() &&
               std::isspace(static_cast<unsigned char>(body[nameEnd])) == 0) {
            ++nameEnd;
        }
        tag.element = body.substr(0, nameEnd);
        tag.attributes = body.substr(nameEnd);
        return tag;
    }
}

bool flagged(std::string_view attributes, std::string_view key) {
    return attribute(attributes, key) == "yes";
}

/// The ROM a `rom` tag names, or nothing when nothing could be checked against it.
std::optional<DatRom> romOf(std::string_view attributes) {
    if (attribute(attributes, "status") == "nodump" || flagged(attributes, "optional")) {
        return std::nullopt;
    }
    const std::optional<std::string> crc = attribute(attributes, "crc");
    const std::optional<std::string> size = attribute(attributes, "size");
    if (!crc || !size) {
        return std::nullopt;
    }
    const std::optional<std::uint32_t> crcValue = numberOf<std::uint32_t>(*crc, 16);
    const std::optional<std::uint64_t> sizeValue = numberOf<std::uint64_t>(*size, 10);
    if (!crcValue || !sizeValue) {
        return std::nullopt;
    }
    return DatRom{attributeOr(attributes, "name"), *sizeValue, *crcValue,
                  attributeOr(attributes, "bios"), false};
}

/// A stick's directions from `<control ways=...>` or a type naming them (`joy8way`, `vjoy2way`).
void readWays(std::string_view text, DatControls& controls) {
    if (text.find("vertical2") != std::string_view::npos ||
        text.find("vjoy2way") != std::string_view::npos) {
        controls.ways = 2;
        controls.vertical = true;
        return;
    }
    for (const unsigned ways : {8U, 4U, 2U}) {
        if (text == std::to_string(ways) ||
            text.find(std::format("joy{}way", ways)) != std::string_view::npos) {
            controls.ways = ways;
            return;
        }
    }
}

/// What an `<input>` or `<control>` tag adds; the first stick and the most buttons named stand.
void readControls(std::string_view attributes, DatControls& controls) {
    if (const std::optional<std::string> buttons = attribute(attributes, "buttons")) {
        controls.buttons = std::max(controls.buttons, numberOf<unsigned>(*buttons, 10).value_or(0));
    }
    if (controls.ways != 0) {
        return;
    }
    readWays(attributeOr(attributes, "ways"), controls);
    if (controls.ways == 0) {
        readWays(attributeOr(attributes, "type"), controls);
    }
    if (controls.ways == 0) {
        readWays(attributeOr(attributes, "control"), controls);
    }
}

bool isSetElement(std::string_view element) {
    return element == "game" || element == "machine";
}

/// The tab-separated fields of one line.
std::vector<std::string_view> fieldsOf(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find(field, start);
        fields.push_back(line.substr(start, end - start));
        if (end == std::string_view::npos) {
            return fields;
        }
        start = end + 1;
    }
}

} // namespace

void RomsetDat::add(DatSet set, const Wanted& wanted) {
    if (links_.contains(set.name)) {
        return;
    }
    links_.emplace(set.name, DatLink{set.romof, set.defaultBios});
    // A device matters only for the ROMs it or the devices it uses need.
    const bool needs = !set.device || !set.roms.empty() || !set.devices.empty();
    if (needs && (wanted.empty() || wanted.contains(set.name) || set.device)) {
        index_.emplace(set.name, sets_.size());
        sets_.push_back(std::move(set));
    }
}

void RomsetDat::markChoices() {
    // FinalBurn's datafiles write no `biosset`: they list every BIOS a player could pick as a ROM.
    if (std::ranges::any_of(links_, [](const auto& link) {
            return !link.second.defaultBios.empty();
        })) {
        return;
    }
    for (DatSet& set : sets_) {
        for (const std::string& name : chainOf(set.name)) {
            const DatSet* owner = find(name);
            if (owner == nullptr || !owner->bios) {
                continue;
            }
            for (DatRom& rom : set.roms) {
                rom.choice = std::ranges::any_of(owner->roms, [&rom](const DatRom& offered) {
                    return offered.crc32 == rom.crc32 && offered.size == rom.size;
                });
            }
        }
    }
}

void RomsetDat::addXml(std::string_view xml, const Wanted& wanted) {
    std::size_t at = 0;
    std::optional<DatSet> set;
    std::string firstBios;
    const auto finish = [&] {
        if (set) {
            if (set->defaultBios.empty()) {
                set->defaultBios = firstBios;
            }
            add(std::move(*set), wanted);
            set.reset();
        }
    };
    while (const std::optional<Tag> tag = nextTag(xml, at)) {
        if (isSetElement(tag->element)) {
            if (tag->closing) {
                finish();
                continue;
            }
            finish();
            set = DatSet{};
            firstBios.clear();
            set->name = lower(attributeOr(tag->attributes, "name"));
            set->romof = lower(attributeOr(tag->attributes, "romof"));
            set->device = flagged(tag->attributes, "isdevice");
            set->bios = flagged(tag->attributes, "isbios");
            if (tag->empty) {
                finish();
            }
            continue;
        }
        if (!set || tag->closing) {
            continue;
        }
        if (tag->element == "rom") {
            if (std::optional<DatRom> rom = romOf(tag->attributes)) {
                set->roms.push_back(std::move(*rom));
            }
        } else if (tag->element == "biosset") {
            const std::string name = attributeOr(tag->attributes, "name");
            if (firstBios.empty()) {
                firstBios = name;
            }
            if (flagged(tag->attributes, "default")) {
                set->defaultBios = name;
            }
        } else if (tag->element == "device_ref") {
            const std::string name = lower(attributeOr(tag->attributes, "name"));
            if (!name.empty() && std::ranges::find(set->devices, name) == set->devices.end()) {
                set->devices.push_back(name);
            }
        } else if (tag->element == "input" || tag->element == "control") {
            readControls(tag->attributes, set->controls);
        } else if (tag->element == "disk" && attribute(tag->attributes, "status") != "nodump" &&
                   !flagged(tag->attributes, "optional")) {
            set->disks.push_back(attributeOr(tag->attributes, "name"));
        }
    }
    finish();
    markChoices();
}

std::string RomsetDat::serialize() const {
    std::string out;
    for (const auto& [name, link] : links_) {
        const auto found = index_.find(name);
        if (found != index_.end()) {
            continue;
        }
        out += std::format("L\t{}\t{}\t{}\n", name, link.romof, link.defaultBios);
    }
    for (const DatSet& set : sets_) {
        out += std::format("S\t{}\t{}\t{}\t{}\n", set.name, set.romof, set.defaultBios,
                           set.device ? "d" : (set.bios ? "b" : "-"));
        for (const DatRom& rom : set.roms) {
            out += std::format("R\t{:08x}\t{}\t{}\t{}\t{}\n", rom.crc32, rom.size, rom.bios,
                               rom.choice ? "c" : "-", rom.name);
        }
        for (const std::string& device : set.devices) {
            out += std::format("V\t{}\n", device);
        }
        for (const std::string& disk : set.disks) {
            out += std::format("K\t{}\n", disk);
        }
        if (set.controls != DatControls{}) {
            out += std::format("I\t{}\t{}\t{}\n", set.controls.buttons, set.controls.ways,
                               set.controls.vertical ? "v" : "-");
        }
    }
    return out;
}

std::optional<RomsetDat> RomsetDat::parse(std::string_view text, const Wanted& wanted,
                                          std::string& error) {
    RomsetDat dat;
    std::optional<DatSet> set;
    std::size_t number = 0;
    const auto finish = [&] {
        if (set) {
            dat.add(std::move(*set), wanted);
            set.reset();
        }
    };
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string_view line = text.substr(at, end - at);
        at = end + 1;
        ++number;
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string_view> fields = fieldsOf(line);
        const std::string_view kind = fields.front();
        const auto malformed = [&] {
            error = std::format("line {} is malformed", number);
            return std::nullopt;
        };
        if (kind == "L" && fields.size() == 4) {
            finish();
            dat.links_.try_emplace(std::string{fields[1]},
                                   DatLink{std::string{fields[2]}, std::string{fields[3]}});
        } else if (kind == "S" && fields.size() == 5) {
            finish();
            set = DatSet{std::string{fields[1]},
                         std::string{fields[2]},
                         std::string{fields[3]},
                         fields[4] == "d",
                         fields[4] == "b",
                         {},
                         {},
                         {},
                         {}};
        } else if (kind == "R" && fields.size() == 6 && set) {
            const std::optional<std::uint32_t> crc = numberOf<std::uint32_t>(fields[1], 16);
            const std::optional<std::uint64_t> size = numberOf<std::uint64_t>(fields[2], 10);
            if (!crc || !size) {
                return malformed();
            }
            set->roms.push_back(DatRom{std::string{fields[5]}, *size, *crc, std::string{fields[3]},
                                       fields[4] == "c"});
        } else if (kind == "V" && fields.size() == 2 && set) {
            set->devices.emplace_back(fields[1]);
        } else if (kind == "K" && fields.size() == 2 && set) {
            set->disks.emplace_back(fields[1]);
        } else if (kind == "I" && fields.size() == 4 && set) {
            const std::optional<unsigned> buttons = numberOf<unsigned>(fields[1], 10);
            const std::optional<unsigned> ways = numberOf<unsigned>(fields[2], 10);
            if (!buttons || !ways) {
                return malformed();
            }
            set->controls = DatControls{*buttons, *ways, fields[3] == "v"};
        } else {
            return malformed();
        }
    }
    finish();
    return dat;
}

const DatSet* RomsetDat::find(std::string_view name) const {
    const auto found = index_.find(name);
    return found == index_.end() ? nullptr : &sets_[found->second];
}

const DatLink* RomsetDat::link(std::string_view name) const {
    const auto found = links_.find(name);
    return found == links_.end() ? nullptr : &found->second;
}

std::vector<std::string> RomsetDat::chainOf(std::string_view name) const {
    std::vector<std::string> chain{std::string{name}};
    for (const DatLink* next = link(name);
         next != nullptr && !next->romof.empty() && chain.size() < maxChain &&
         std::ranges::find(chain, next->romof) == chain.end();
         next = link(chain.back())) {
        chain.push_back(next->romof);
    }
    return chain;
}

} // namespace opensu::library::roms

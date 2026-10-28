#include "switch_keys.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>

namespace opensu::library::roms {
namespace {

namespace fs = std::filesystem;

/// A keys file is a few kilobytes; anything far larger is not one.
constexpr std::uintmax_t largestKeysFile = std::uintmax_t{4} * 1024 * 1024;

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const int lowered = std::tolower(static_cast<unsigned char>(c));
    if (lowered >= 'a' && lowered <= 'f') {
        return lowered - 'a' + 10;
    }
    return -1;
}

std::optional<std::vector<std::uint8_t>> hexBytes(std::string_view hex) {
    if (hex.empty() || hex.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int high = hexDigit(hex[i]);
        const int low = hexDigit(hex[i + 1]);
        if (high < 0 || low < 0) {
            secrets::wipeBytes(bytes.data(), bytes.size());
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

std::string lowered(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/// Each `name = hex` line of a keys file, the name lower-cased.
template <class Take> void eachLine(std::string_view text, Take take) {
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        const std::string_view name = trimmed(line.substr(0, equals));
        std::optional<std::vector<std::uint8_t>> value = hexBytes(trimmed(line.substr(equals + 1)));
        if (!name.empty() && value) {
            take(lowered(name), std::move(*value));
        }
    }
}

std::string withRevision(std::string_view stem, std::uint8_t revision) {
    std::array<char, 4> digits{};
    std::snprintf(digits.data(), digits.size(), "%02x", static_cast<unsigned>(revision));
    return std::string{stem} + digits.data();
}

std::string_view areaName(KeyArea area) {
    switch (area) {
    case KeyArea::Application:
        return "application";
    case KeyArea::Ocean:
        return "ocean";
    case KeyArea::System:
        return "system";
    }
    return "application";
}

template <std::size_t N> std::optional<KeyBytes<N>> fixed(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != N) {
        return std::nullopt;
    }
    KeyBytes<N> key;
    std::ranges::copy(bytes, key.bytes().begin());
    return key;
}

/// `source` decrypted by `key`, one 16-byte block.
std::optional<Key128> unwrapped(const Key128& key, const Key128& source) {
    Key128 out = source;
    if (!decryptEcb(key, out.bytes())) {
        return std::nullopt;
    }
    return out;
}

std::string readSmall(const fs::path& file, std::string& error) {
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(file, ec);
    if (ec || size > largestKeysFile) {
        error = file.string() + ": " + (ec ? ec.message() : "too large for a keys file");
        return {};
    }
    std::ifstream in{file, std::ios::binary};
    if (!in) {
        error = file.string() + ": cannot be opened";
        return {};
    }
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

} // namespace

SwitchKeys::~SwitchKeys() {
    for (auto& entry : keys_) {
        secrets::wipeBytes(entry.second.data(), entry.second.size());
    }
}

SwitchKeys SwitchKeys::parse(std::string_view prodKeys, std::string_view titleKeys) {
    SwitchKeys keys;
    eachLine(prodKeys, [&keys](std::string name, std::vector<std::uint8_t> value) {
        if (auto [at, added] = keys.keys_.try_emplace(std::move(name), std::move(value)); !added) {
            secrets::wipeBytes(value.data(), value.size());
        }
    });
    eachLine(titleKeys, [&keys](const std::string& name, std::vector<std::uint8_t> value) {
        const std::optional<std::vector<std::uint8_t>> rights = hexBytes(name);
        std::optional<Key128> key = fixed<16>(value);
        secrets::wipeBytes(value.data(), value.size());
        if (rights && rights->size() == RightsId{}.size() && key) {
            RightsId id{};
            std::ranges::copy(*rights, id.begin());
            keys.titleKeys_.try_emplace(id, std::move(*key));
        }
    });
    return keys;
}

std::optional<SwitchKeys> SwitchKeys::load(const fs::path& dir, std::string& error) {
    std::error_code ec;
    const fs::path prod = dir / "prod.keys";
    if (!fs::is_regular_file(prod, ec)) {
        error = prod.string() + ": not there";
        return std::nullopt;
    }
    std::string prodText = readSmall(prod, error);
    if (prodText.empty()) {
        if (error.empty()) {
            error = prod.string() + ": empty";
        }
        return std::nullopt;
    }
    std::string titleText;
    if (const fs::path title = dir / "title.keys"; fs::is_regular_file(title, ec)) {
        std::string ignored;
        titleText = readSmall(title, ignored);
    }
    SwitchKeys keys = parse(prodText, titleText);
    secrets::wipeBytes(prodText.data(), prodText.size());
    secrets::wipeBytes(titleText.data(), titleText.size());
    return keys;
}

std::optional<Key128> SwitchKeys::named(const std::string& name) const {
    const auto found = keys_.find(name);
    return found == keys_.end() ? std::nullopt : fixed<16>(found->second);
}

std::string SwitchKeys::keyAreaKeyName(KeyArea area, std::uint8_t revision) {
    return withRevision("key_area_key_" + std::string{areaName(area)} + "_", revision);
}

std::string SwitchKeys::titleKekName(std::uint8_t revision) {
    return withRevision("titlekek_", revision);
}

std::optional<Key256> SwitchKeys::headerKey() const {
    if (const auto found = keys_.find("header_key"); found != keys_.end()) {
        return fixed<32>(found->second);
    }
    // header_kek = generate_kek(header_kek_source, master_key_00), then the 32-byte source.
    const std::optional<Key128> master = named("master_key_00");
    const std::optional<Key128> kekSeed = named("aes_kek_generation_source");
    const std::optional<Key128> keySeed = named("aes_key_generation_source");
    const std::optional<Key128> kekSource = named("header_kek_source");
    const auto source = keys_.find("header_key_source");
    if (!master || !kekSeed || !keySeed || !kekSource || source == keys_.end()) {
        return std::nullopt;
    }
    std::optional<Key128> kek = unwrapped(*master, *kekSeed);
    std::optional<Key128> sourceKek = kek ? unwrapped(*kek, *kekSource) : std::nullopt;
    std::optional<Key128> headerKek = sourceKek ? unwrapped(*sourceKek, *keySeed) : std::nullopt;
    std::optional<Key256> key = fixed<32>(source->second);
    if (!headerKek || !key || !decryptEcb(*headerKek, key->bytes())) {
        return std::nullopt;
    }
    return key;
}

std::optional<Key128> SwitchKeys::keyAreaKey(KeyArea area, std::uint8_t revision) const {
    if (std::optional<Key128> key = named(keyAreaKeyName(area, revision))) {
        return key;
    }
    const std::optional<Key128> master = named(withRevision("master_key_", revision));
    const std::optional<Key128> kekSeed = named("aes_kek_generation_source");
    const std::optional<Key128> keySeed = named("aes_key_generation_source");
    const std::optional<Key128> source =
        named("key_area_key_" + std::string{areaName(area)} + "_source");
    if (!master || !kekSeed || !keySeed || !source) {
        return std::nullopt;
    }
    const std::optional<Key128> kek = unwrapped(*master, *kekSeed);
    const std::optional<Key128> sourceKek = kek ? unwrapped(*kek, *source) : std::nullopt;
    return sourceKek ? unwrapped(*sourceKek, *keySeed) : std::nullopt;
}

std::optional<Key128> SwitchKeys::titleKek(std::uint8_t revision) const {
    if (std::optional<Key128> key = named(titleKekName(revision))) {
        return key;
    }
    const std::optional<Key128> master = named(withRevision("master_key_", revision));
    const std::optional<Key128> source = named("titlekek_source");
    if (!master || !source) {
        return std::nullopt;
    }
    return unwrapped(*master, *source);
}

std::optional<Key128> SwitchKeys::titleKey(const RightsId& rights) const {
    const auto found = titleKeys_.find(rights);
    return found == titleKeys_.end() ? std::nullopt : std::optional<Key128>{found->second};
}

} // namespace opensu::library::roms

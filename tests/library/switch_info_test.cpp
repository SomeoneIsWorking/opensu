// The Switch reader: NSP and XCI files built here and encrypted the way Nintendo's are, with
// made-up keys, read through SwitchInfo and the ROM provider.
#include "library/switch_info.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <openssl/evp.h>

#include "library/roms.hpp"
#include "library/switch_keys.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::library::Game;
using opensu::library::roms::DumpInfo;
using opensu::library::roms::KeyArea;
using opensu::library::roms::SwitchInfo;
using opensu::library::roms::SwitchKeys;
using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
}

void expect(bool condition, const char* what) {
    if (!condition) {
        fail(what);
    }
}

template <class T> const T& need(const std::optional<T>& value, const char* what) {
    if (!value) {
        fail(what);
    }
    return *value;
}

Bytes hex(std::string_view text) {
    Bytes out;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        out.push_back(
            static_cast<std::uint8_t>(std::stoi(std::string{text.substr(i, 2)}, nullptr, 16)));
    }
    return out;
}

// Made-up keys; none is a real console's.
constexpr std::string_view headerKey =
    "4bcfe067deea358584c08c0b1dae949d66ec036f1116bfaf902a19dc617bcff2";
constexpr std::string_view keyAreaKey = "f3c664ff72e5e68c817eb5cd32faec02";
constexpr std::string_view titleKek = "212d0d964f5efaa05d5113845e2a9b3e";
constexpr std::string_view sectionKey = "8899aabbccddeeff0011223344556677";
/// The sources the three keys above are derived from (computed with `openssl enc`).
constexpr std::string_view masterAndSources = "master_key_00 = 000102030405060708090a0b0c0d0e0f\n"
                                              "master_key_02 = 000102030405060708090a0b0c0d0e0f\n"
                                              "aes_kek_generation_source = "
                                              "101112131415161718191a1b1c1d1e1f\n"
                                              "aes_key_generation_source = "
                                              "202122232425262728292a2b2c2d2e2f\n"
                                              "key_area_key_application_source = "
                                              "303132333435363738393a3b3c3d3e3f\n"
                                              "titlekek_source = 404142434445464748494a4b4c4d4e4f\n"
                                              "header_kek_source = "
                                              "505152535455565758595a5b5c5d5e5f\n"
                                              "header_key_source = "
                                              "606162636465666768696a6b6c6d6e6f"
                                              "707172737475767778797a7b7c7d7e7f\n";
/// key generation 3 is master key revision 2.
constexpr std::uint8_t generation = 3;

Bytes crypt(const EVP_CIPHER* cipher, const Bytes& key, const std::uint8_t* iv, Bytes data) {
    EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
    int written = 0;
    int tail = 0;
    const bool done = EVP_EncryptInit_ex(context, cipher, nullptr, key.data(), iv) == 1 &&
                      EVP_CIPHER_CTX_set_padding(context, 0) == 1 &&
                      EVP_EncryptUpdate(context, data.data(), &written, data.data(),
                                        static_cast<int>(data.size())) == 1 &&
                      EVP_EncryptFinal_ex(context, data.data() + written, &tail) == 1;
    EVP_CIPHER_CTX_free(context);
    expect(done, "the fixture encrypts");
    return data;
}

void putLe(Bytes& bytes, std::size_t at, std::uint64_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) {
        bytes[at + i] = static_cast<std::uint8_t>((value >> (CHAR_BIT * i)) & 0xffU);
    }
}

void putText(Bytes& bytes, std::size_t at, std::string_view text) {
    std::ranges::copy(text, bytes.begin() + static_cast<std::ptrdiff_t>(at));
}

void append(Bytes& to, const Bytes& more) {
    to.insert(to.end(), more.begin(), more.end());
}

std::size_t padded(std::size_t size, std::size_t unit) {
    return (size + unit - 1) / unit * unit;
}

/// A control.nacp with `titles` by language index.
Bytes nacp(const std::map<std::size_t, std::string>& titles) {
    Bytes out(0x4000);
    for (const auto& [language, title] : titles) {
        putText(out, language * 0x300, title);
    }
    return out;
}

/// A RomFS holding `files` at its root.
Bytes romfs(const std::vector<std::pair<std::string, Bytes>>& files) {
    Bytes table;
    Bytes data;
    for (const auto& [name, contents] : files) {
        Bytes entry(0x20 + padded(name.size(), 4));
        putLe(entry, 0x8, data.size(), 8);
        putLe(entry, 0x10, contents.size(), 8);
        putLe(entry, 0x1C, name.size(), 4);
        putText(entry, 0x20, name);
        append(table, entry);
        append(data, contents);
        data.resize(padded(data.size(), 16));
    }
    Bytes out(0x50);
    const std::size_t dataAt = padded(0x50 + table.size(), 16);
    putLe(out, 0, 0x50, 8);
    putLe(out, 0x38, 0x50, 8);
    putLe(out, 0x40, table.size(), 8);
    putLe(out, 0x48, dataAt, 8);
    append(out, table);
    out.resize(dataAt);
    append(out, data);
    return out;
}

struct NcaSpec {
    std::uint8_t content{2};
    std::uint64_t programId{0x0100000000001000};
    /// A rights id makes the section key a title key.
    std::optional<Bytes> rights;
    Bytes romfs;
};

/// An NCA3 as a package carries it: header in AES-XTS, the RomFS section in AES-CTR.
Bytes nca(const NcaSpec& spec) {
    constexpr std::size_t start = 0xC00;
    constexpr std::size_t dataLevel = 0x200;
    Bytes section(dataLevel);
    append(section, spec.romfs);
    section.resize(padded(section.size(), 0x200));

    Bytes header(start);
    putText(header, 0x200, "NCA3");
    header[0x205] = spec.content;
    putLe(header, 0x210, spec.programId, 8);
    header[0x220] = generation;
    putLe(header, 0x240, start / 0x200, 4);
    putLe(header, 0x244, (start + section.size()) / 0x200, 4);
    const std::size_t fs = 0x400;
    header[fs + 0x3] = 3;
    header[fs + 0x4] = 3;
    putText(header, fs + 0x8, "IVFC");
    putLe(header, fs + 0x90, dataLevel, 8);
    putLe(header, fs + 0x98, spec.romfs.size(), 8);
    const std::array<std::uint8_t, 8> stored{1, 2, 3, 4, 5, 6, 7, 8};
    std::ranges::copy(stored, header.begin() + fs + 0x140);
    if (spec.rights) {
        std::ranges::copy(*spec.rights, header.begin() + 0x230);
    } else {
        Bytes keys(0x40);
        std::ranges::copy(hex(sectionKey), keys.begin() + 0x20);
        keys = crypt(EVP_aes_128_ecb(), hex(keyAreaKey), nullptr, keys);
        std::ranges::copy(keys, header.begin() + 0x300);
    }

    std::array<std::uint8_t, 16> counter{};
    std::reverse_copy(stored.begin(), stored.end(), counter.begin());
    std::uint64_t block = start / 16;
    for (std::size_t i = 16; i > 8; --i) {
        counter[i - 1] = static_cast<std::uint8_t>(block & 0xffU);
        block >>= CHAR_BIT;
    }
    Bytes out;
    for (std::size_t sector = 0; sector < start / 0x200; ++sector) {
        std::array<std::uint8_t, 16> tweak{};
        tweak[15] = static_cast<std::uint8_t>(sector);
        append(out,
               crypt(EVP_aes_128_xts(), hex(headerKey), tweak.data(),
                     Bytes(header.begin() + static_cast<std::ptrdiff_t>(sector * 0x200),
                           header.begin() + static_cast<std::ptrdiff_t>((sector + 1) * 0x200))));
    }
    append(out, crypt(EVP_aes_128_ctr(), hex(sectionKey), counter.data(), section));
    return out;
}

/// A PFS0 (`hashed` false) or HFS0 of `files`.
Bytes partition(const std::vector<std::pair<std::string, Bytes>>& files, bool hashed) {
    const std::size_t entrySize = hashed ? 0x40 : 0x18;
    std::string names;
    Bytes table(files.size() * entrySize);
    std::uint64_t offset = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        putLe(table, i * entrySize, offset, 8);
        putLe(table, i * entrySize + 8, files[i].second.size(), 8);
        putLe(table, i * entrySize + 16, names.size(), 4);
        names += files[i].first + '\0';
        offset += files[i].second.size();
    }
    names.resize(padded(names.size(), 0x10));
    Bytes out(0x10);
    putText(out, 0, hashed ? "HFS0" : "PFS0");
    putLe(out, 4, files.size(), 4);
    putLe(out, 8, names.size(), 4);
    append(out, table);
    out.insert(out.end(), names.begin(), names.end());
    for (const auto& file : files) {
        append(out, file.second);
    }
    return out;
}

/// A card image whose secure partition holds `files`, with the key area in front if `keyArea`.
Bytes xci(const std::vector<std::pair<std::string, Bytes>>& files, bool keyArea) {
    Bytes card(keyArea ? 0x1000 : 0);
    Bytes header(0x200);
    putText(header, 0x100, "HEAD");
    putLe(header, 0x130, 0x200, 8);
    append(card, header);
    append(card,
           partition({{"update", partition({}, true)}, {"secure", partition(files, true)}}, true));
    return card;
}

/// A common ticket (RSA-2048 SHA-256 signed) holding `encrypted` for `rights`.
Bytes ticket(const Bytes& rights, const Bytes& encrypted) {
    constexpr std::size_t header = 4 + 0x100 + 0x3C;
    Bytes out(header + 0x180);
    putLe(out, 0, 0x10004, 4);
    std::ranges::copy(encrypted, out.begin() + header + 0x40);
    std::ranges::copy(rights, out.begin() + header + 0x160);
    return out;
}

Bytes jpeg() {
    return {0xFF, 0xD8, 0xFF, 0xE0, 'f', 'a', 'k', 'e'};
}

Bytes controlRomfs(const std::map<std::size_t, std::string>& titles,
                   const std::string& iconName = "icon_AmericanEnglish.dat") {
    return romfs({{"control.nacp", nacp(titles)}, {iconName, jpeg()}});
}

void writeFile(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

void writeText(const fs::path& path, std::string_view text) {
    writeFile(path, Bytes(text.begin(), text.end()));
}

Bytes readFile(const fs::path& path) {
    std::ifstream in{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// prod.keys with the derived keys given outright.
std::string derivedKeys() {
    return "header_key = " + std::string{headerKey} +
           "\nkey_area_key_application_02 = " + std::string{keyAreaKey} +
           "\ntitlekek_02 = " + std::string{titleKek} + "\n";
}

struct Rig {
    opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "switch-info"};
    fs::path keys = scratch.path() / "keys";
    fs::path cache = scratch.path() / "cache";
    fs::path roms = scratch.path() / "roms";

    [[nodiscard]] SwitchInfo info() const {
        return SwitchInfo{{scratch.path() / "absent", keys}, cache};
    }
};

void derivesMissingKeys() {
    const SwitchKeys given = SwitchKeys::parse(derivedKeys() + std::string{masterAndSources}, "");
    const SwitchKeys derived = SwitchKeys::parse(masterAndSources, "");
    const auto same = [](const auto& a, const auto& b) {
        return a && b && std::ranges::equal(a->bytes(), b->bytes());
    };
    expect(same(given.headerKey(), derived.headerKey()), "header_key is derived from master key 0");
    expect(same(given.keyAreaKey(KeyArea::Application, 2),
                derived.keyAreaKey(KeyArea::Application, 2)),
           "a key area key is derived from its master key");
    expect(same(given.titleKek(2), derived.titleKek(2)),
           "a titlekek is derived from its master key");
    expect(!derived.keyAreaKey(KeyArea::Application, 3) && !derived.titleKek(1),
           "a revision whose master key is missing has no key");
    expect(SwitchKeys::keyAreaKeyName(KeyArea::Ocean, 0x12) == "key_area_key_ocean_12",
           "refusals name the key by prod.keys' name");
}

void readsAnNsp() {
    const Rig rig;
    writeText(rig.keys / "prod.keys", derivedKeys());
    const fs::path game = rig.roms / "Kirby [0100000000001000].nsp";
    writeFile(game,
              partition({{"meta.cnmt.nca", Bytes(0xC00)},
                         {"program.nca", nca({.content = 0, .romfs = romfs({})})},
                         {"update.nca", nca({.programId = 0x0100000000001800,
                                             .romfs = controlRomfs({{0, "Update"}})})},
                         {"control.nca", nca({.romfs = controlRomfs({{0, "Kirby™ Star Allies"},
                                                                     {2, "星のカービィ"}})})}},
                        false));
    SwitchInfo info = rig.info();
    info.begin();
    const DumpInfo read = need(info.read(game), "the NSP reads");
    expect(read.title == "Kirby Star Allies",
           "the base title's control.nacp names the game in American English, trademarks dropped");
    expect(!read.icon.empty() && read.icon.parent_path() == rig.cache &&
               read.icon.extension() == ".jpg" && readFile(read.icon) == jpeg(),
           "its icon is kept in the cache as the JPEG it is");
    info.finish();

    // Unchanged, the file is not opened again: the cache answers with no keys there at all.
    SwitchInfo keyless{{rig.scratch.path() / "absent"}, rig.cache};
    keyless.begin();
    const DumpInfo again = need(keyless.read(game), "the cache answers");
    expect(again.title == "Kirby Star Allies" && again.icon == read.icon,
           "an unchanged file is answered from the cache");
    writeFile(game, Bytes(16));
    expect(!keyless.read(game), "a changed file is read again");
}

void readsAnXciWithDerivedKeys() {
    const Rig rig;
    writeText(rig.keys / "prod.keys", masterAndSources);
    const Bytes control = nca({.romfs = controlRomfs({{2, "Japanese Only"}}, "icon_Japanese.dat")});
    for (const bool keyArea : {false, true}) {
        const fs::path game = rig.roms / (keyArea ? "full.xci" : "trimmed.xci");
        writeFile(game, xci({{"control.nca", control}}, keyArea));
        SwitchInfo info = rig.info();
        info.begin();
        const std::optional<DumpInfo> read = info.read(game);
        expect(read && read->title == "Japanese Only" && readFile(read->icon) == jpeg(),
               "an XCI's secure partition is read, with keys derived from master keys, the first "
               "title and icon there taken");
    }
}

void readsTitleKeyNcas() {
    const Rig rig;
    writeText(rig.keys / "prod.keys", derivedKeys());
    const Bytes rights = hex("0100000000001000000000000000000c");
    const Bytes encrypted = crypt(EVP_aes_128_ecb(), hex(titleKek), nullptr, hex(sectionKey));
    const Bytes control = nca({.rights = rights, .romfs = controlRomfs({{0, "Ticketed"}})});

    const fs::path ticketed = rig.roms / "ticketed.nsp";
    writeFile(ticketed,
              partition({{"control.nca", control},
                         {"0100000000001000000000000000000c.tik", ticket(rights, encrypted)}},
                        false));
    SwitchInfo info = rig.info();
    info.begin();
    expect(info.read(ticketed).has_value(), "the title key comes from the package's ticket");

    const fs::path bare = rig.roms / "bare.nsp";
    writeFile(bare, partition({{"control.nca", control}}, false));
    expect(!info.read(bare), "without a ticket or title.keys there is no title key");
    std::string line = "0100000000001000000000000000000c = ";
    for (const std::uint8_t b : encrypted) {
        std::array<char, 3> digits{};
        std::snprintf(digits.data(), digits.size(), "%02x", b);
        line += digits.data();
    }
    writeText(rig.keys / "title.keys", line + "\n");
    SwitchInfo withTitleKeys = rig.info();
    withTitleKeys.begin();
    const std::optional<DumpInfo> read = withTitleKeys.read(bare);
    expect(read && read->title == "Ticketed", "the title key comes from title.keys");
}

void refusesWhatDoesNotRead() {
    const Rig rig;
    writeText(rig.keys / "prod.keys",
              "header_key = " + std::string(64, '1') +
                  "\nkey_area_key_application_02 = " + std::string{keyAreaKey} + "\n");
    const fs::path game = rig.roms / "game.nsp";
    writeFile(game, partition({{"control.nca", nca({.romfs = controlRomfs({{0, "X"}})})}}, false));
    SwitchInfo info = rig.info();
    info.begin();
    expect(!info.read(game), "a header key that does not decrypt the header reads nothing");
    const fs::path junk = rig.roms / "junk.nsp";
    writeFile(junk, Bytes(0x400, 0x5A));
    expect(!info.read(junk), "a file that is no package reads nothing");
    expect(!fs::exists(rig.cache) || fs::is_empty(rig.cache), "nothing is cached for a failure");
    expect(!SwitchInfo{}.read(game), "a reader with no key folders reads nothing");
}

void titlesProviderGames() {
    const Rig rig;
    writeText(rig.keys / "prod.keys", derivedKeys());
    const fs::path folder = rig.roms / "Switch" / "Kirby Star Allies";
    writeFile(folder / "Kirby Star Allies [0100000000001000][v0].nsp",
              partition({{"control.nca", nca({.romfs = controlRomfs({{0, "Kirby® Star "
                                                                         "Allies"}})})}},
                        false));
    writeFile(folder / "Kirby Star Allies [0100000000001800][v327680].nsp", Bytes(0x10000));
    writeText(folder / "site.url", "x");
    opensu::library::roms::Provider provider{
        {rig.roms},
        opensu::library::roms::EmulatorSetup{
            opensu::library::roms::EmulatorCatalog::shipped(OPENSU_ASSETS_DIR, "/home/p"),
            opensu::library::roms::EmulatorSearch{},
            {}},
        opensu::library::roms::NameDb{},
        {},
        rig.info()};
    const std::vector<Game> games = provider.list();
    expect(games.size() == 1 && games[0].title == "Kirby Star Allies" &&
               games[0].artworkKey == "Kirby Star Allies" && readFile(games[0].artwork) == jpeg(),
           "a Switch game folder is titled and shown by its base file's own control data");
}

} // namespace

int main() try {
    derivesMissingKeys();
    readsAnNsp();
    readsAnXciWithDerivedKeys();
    readsTitleKeyNcas();
    refusesWhatDoesNotRead();
    titlesProviderGames();
    std::printf("switch_info: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

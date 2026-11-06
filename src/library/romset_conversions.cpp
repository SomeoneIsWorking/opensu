#include "romset_conversions.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <numeric>

#include <nlohmann/json.hpp>

#include "lucent/log.h"

namespace opensu::library::roms {
namespace {

using json = nlohmann::json;

constexpr std::size_t keyBytes = 20;
constexpr std::size_t keyBits = keyBytes * 8;
constexpr std::size_t wordBits = 16;
/// Where a key file's bit stream starts, as MAME's CPS2 decryption reads it.
constexpr std::size_t keyBitOrigin = 317;

ConversionPiece pieceFrom(const json& row) {
    return ConversionPiece{row.at(0).get<std::uint32_t>(), row.at(1).get<std::uint32_t>(),
                           row.at(2).get<std::uint32_t>(), row.at(3).get<std::uint32_t>(),
                           row.at(4).get<std::uint8_t>()};
}

bool isCps2Key(const DatRom& rom) {
    return rom.size == keyBytes && rom.name.ends_with(".key");
}

std::string fileText(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

} // namespace

std::string cps2KeyFile(const Cps2Key& key) {
    std::string file(keyBytes, '\0');
    for (std::size_t b = 0; b < keyBits; ++b) {
        if ((key.at(b / wordBits) & (0x8000U >> (b % wordBits))) != 0) {
            const std::size_t bit = (keyBitOrigin - b) % keyBits;
            file.at(bit / 8) = static_cast<char>(static_cast<unsigned char>(file.at(bit / 8)) |
                                                 (1U << ((bit ^ 7U) % 8)));
        }
    }
    return file;
}

RomsetConversions RomsetConversions::shipped(const std::filesystem::path& assetsDir) {
    RomsetConversions conversions;
    std::string error;
    if (const std::string text = fileText(assetsDir / conversionsFile);
        !text.empty() && !conversions.addConversions(text, error)) {
        lucent::warn("roms", "{}: {}", conversionsFile, error);
    }
    if (const std::string text = fileText(assetsDir / cps2KeysFile);
        !text.empty() && !conversions.addKeys(text, error)) {
        lucent::warn("roms", "{}: {}", cps2KeysFile, error);
    }
    return conversions;
}

bool RomsetConversions::addConversions(std::string_view text, std::string& error) {
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded()) {
        error = "not JSON";
        return false;
    }
    try {
        for (const json& recipe : root.at("recipes")) {
            Recipe pieces;
            for (const json& row : recipe.at("pieces")) {
                pieces.push_back(pieceFrom(row));
            }
            recipes_.emplace(recipe.at("crc").get<std::uint32_t>(), std::move(pieces));
        }
        for (const auto& [zip, targets] : root.at("aliases").items()) {
            aliases_.emplace(zip, targets.get<std::vector<std::string>>());
        }
        for (const json& crc : root.at("optional")) {
            optional_.insert(crc.get<std::uint32_t>());
        }
    } catch (const json::exception& malformed) {
        error = malformed.what();
        return false;
    }
    return true;
}

bool RomsetConversions::addKeys(std::string_view text, std::string& error) {
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded()) {
        error = "not JSON";
        return false;
    }
    try {
        for (const json& row : root.at("keys")) {
            keys_.emplace(row.at("crc").get<std::uint32_t>(), row.at("words").get<Cps2Key>());
        }
    } catch (const json::exception& malformed) {
        error = malformed.what();
        return false;
    }
    return true;
}

const std::vector<std::string>& RomsetConversions::aliasesOf(std::string_view zip) const {
    const auto found = aliases_.find(zip);
    return found == aliases_.end() ? noAliases_ : found->second;
}

RomsetDat::Wanted RomsetConversions::aliasTargets(const RomsetDat::Wanted& zips) const {
    RomsetDat::Wanted targets;
    for (const std::string& zip : zips) {
        const std::vector<std::string>& aliases = aliasesOf(zip);
        targets.insert(aliases.begin(), aliases.end());
    }
    return targets;
}

const RomsetConversions::Recipe* RomsetConversions::recipeFor(const DatRom& rom,
                                                              const HeldFiles& held) const {
    const auto [first, last] = recipes_.equal_range(rom.crc32);
    for (auto recipe = first; recipe != last; ++recipe) {
        const Recipe& pieces = recipe->second;
        const std::uint64_t length =
            std::accumulate(pieces.begin(), pieces.end(), std::uint64_t{0},
                            [](std::uint64_t total, const ConversionPiece& piece) {
                                return total + piece.length;
                            });
        const bool sourced = std::ranges::all_of(pieces, [&](const ConversionPiece& piece) {
            if (piece.source == 0) {
                return true;
            }
            const std::uint64_t last =
                piece.offset + std::uint64_t{piece.length - 1} * piece.stride;
            const auto file = held.lower_bound({piece.source, last + 1});
            return file != held.end() && file->first == piece.source;
        });
        if (length == rom.size && sourced) {
            return &pieces;
        }
    }
    return nullptr;
}

ConversionPlan RomsetConversions::plan(const std::vector<const DatRom*>& needs,
                                       const HeldFiles& held) const {
    ConversionPlan plan;
    bool offersChoice = false;
    bool holdsChoice = false;
    for (const DatRom* rom : needs) {
        offersChoice = offersChoice || rom->choice;
        if (held.contains({rom->crc32, rom->size})) {
            holdsChoice = holdsChoice || rom->choice;
            plan.roms.push_back(PlannedRom{rom, {}, nullptr});
        } else if (const Recipe* recipe = recipeFor(*rom, held)) {
            plan.roms.push_back(PlannedRom{rom, *recipe, nullptr});
            plan.builds = true;
        } else if (const auto key = keys_.find(rom->crc32); key != keys_.end() && isCps2Key(*rom)) {
            plan.roms.push_back(PlannedRom{rom, {}, &key->second});
            plan.builds = true;
        } else if (!rom->choice && !optional_.contains(rom->crc32)) {
            plan.missing.push_back(rom->name);
        }
    }
    if (offersChoice && !holdsChoice) {
        plan.missing.emplace_back("a BIOS");
    }
    return plan;
}

} // namespace opensu::library::roms

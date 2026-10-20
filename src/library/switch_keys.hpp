// switch_keys — the player's Switch keys as an emulator keeps them: prod.keys (`name = hex`) and
// title.keys (`rights id = title key`). Keys a file lacks are derived from the master keys as
// hactool derives them. Key bytes never leave this process: nothing here logs, prints or writes
// them, and they are wiped when the set goes.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "switch_crypto.hpp"

namespace opensu::library::roms {

/// An NCA's rights id: zero for a game card or key-area NCA, else the ticket its title key is in.
using RightsId = std::array<std::uint8_t, 16>;

/// The key area key an NCA's key area is encrypted with (its header's key index).
enum class KeyArea : std::uint8_t {
    Application,
    Ocean,
    System,
};

class SwitchKeys {
  public:
    SwitchKeys() = default;
    SwitchKeys(const SwitchKeys&) = delete;
    SwitchKeys& operator=(const SwitchKeys&) = delete;
    SwitchKeys(SwitchKeys&&) noexcept = default;
    SwitchKeys& operator=(SwitchKeys&&) noexcept = default;
    ~SwitchKeys();

    /// The keys in prod.keys and title.keys text; lines that are not a name and whole hex bytes
    /// are skipped.
    [[nodiscard]] static SwitchKeys parse(std::string_view prodKeys, std::string_view titleKeys);

    /// `prod.keys` and, when it is there, `title.keys` in `dir`. Nothing when `dir` has no
    /// prod.keys, with `error` saying what could not be read.
    [[nodiscard]] static std::optional<SwitchKeys> load(const std::filesystem::path& dir,
                                                        std::string& error);

    /// The NCA header key: `header_key`, else derived from master key 0.
    [[nodiscard]] std::optional<Key256> headerKey() const;

    /// The key area key of `area` for master key `revision`: `key_area_key_<area>_<rev>`, else
    /// derived from `master_key_<rev>`.
    [[nodiscard]] std::optional<Key128> keyAreaKey(KeyArea area, std::uint8_t revision) const;

    /// The title key encryption key for master key `revision`: `titlekek_<rev>`, else derived.
    [[nodiscard]] std::optional<Key128> titleKek(std::uint8_t revision) const;

    /// The encrypted title key title.keys holds for `rights`.
    [[nodiscard]] std::optional<Key128> titleKey(const RightsId& rights) const;

    /// prod.keys' names of the key area key and titlekek of `revision`, for a refusal that says
    /// what the file lacks.
    [[nodiscard]] static std::string keyAreaKeyName(KeyArea area, std::uint8_t revision);
    [[nodiscard]] static std::string titleKekName(std::uint8_t revision);

  private:
    [[nodiscard]] std::optional<Key128> named(const std::string& name) const;

    std::map<std::string, std::vector<std::uint8_t>> keys_;
    std::map<RightsId, Key128> titleKeys_;
};

} // namespace opensu::library::roms

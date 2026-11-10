// cemu_pads — Cemu's controller profiles bound to the virtual pads a game openSU launched reads,
// while it runs. Cemu binds `controllerProfiles/controller<N>.xml` to an SDL pad by its uuid,
// `<index>_<SDL GUID>`, the index counting pads of that GUID in SDL's order (the event number),
// so a profile made with the player's own pad matches none of openSU's virtual pads. Each profile
// with an SDL controller is bound to the Nth virtual pad, its mappings kept; the player's file
// stays beside it as `controller<N>.xml.opensu-player` and is put back as it was when this ends,
// or at the next Cemu launch after a crash.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "emulator_catalog.hpp"
#include "game.hpp"

namespace opensu::library {

/// The catalog's Cemu.
inline constexpr std::string_view cemuId{"cemu"};

/// `profile` with every SDL controller's uuid made `uuid`; the rest byte for byte.
[[nodiscard]] std::string reboundCemuProfile(std::string_view profile, std::string_view uuid);

/// What a Cemu launch binds its profiles to.
struct CemuBinding {
    /// Cemu's settings folder, which holds `controllerProfiles`.
    std::filesystem::path config;
    /// The SDL GUID of the virtual pads the game reads.
    std::string padGuid;
};

/// The binding `game`'s launch needs when it runs on Cemu reading virtual pads of `padGuid`;
/// nothing when it runs on another emulator or reads the pads themselves (`padGuid` empty).
[[nodiscard]] std::optional<CemuBinding>
cemuBindingOf(const Game& game, const roms::EmulatorCatalog& catalog, std::string_view padGuid);

class CemuPads {
  public:
    /// The marker the player's file is kept under while Cemu runs.
    static constexpr std::string_view keptSuffix{".opensu-player"};
    /// Cemu's emulated controllers.
    static constexpr int controllers{8};

    /// Puts back what a crashed run left, then binds the profiles. A profile that cannot be
    /// bound stays the player's and is logged.
    explicit CemuPads(const CemuBinding& binding);
    CemuPads(const CemuPads&) = delete;
    CemuPads& operator=(const CemuPads&) = delete;
    CemuPads(CemuPads&&) = delete;
    CemuPads& operator=(CemuPads&&) = delete;
    /// Puts the player's profiles back.
    ~CemuPads();

  private:
    /// Puts back every player's profile a crashed run left kept in `profiles`.
    static void restore(const std::filesystem::path& profiles);

    std::filesystem::path profiles_;
    std::vector<std::filesystem::path> bound_;
};

} // namespace opensu::library

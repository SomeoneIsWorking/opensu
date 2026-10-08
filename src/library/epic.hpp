// epic — lists Epic Games Store titles from the local Legendary installation.
//
// Legendary owns authentication and downloads; this reads the account's owned titles
// (`list --json`) and which of them are installed (`list-installed --json`).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace iideck::library::epic {

/// How far an install has got, 0 to 1, from one line of `legendary install` output
/// ("[DLManager] INFO: = Progress: 12.34% (505/4096), Running for 00:00:10, ETA: 00:01:11");
/// nothing for any other line.
[[nodiscard]] std::optional<double> installProgress(std::string_view line);

/// Why an install failed, from one line of `legendary install` output: an ERROR or CRITICAL log
/// line, or a " ! Failure: ..." line of its requirements check; nothing for any other line.
[[nodiscard]] std::optional<std::string> installFailure(std::string_view line);

/// Runs Legendary and parses its install list.
class Provider final : public library::Provider {
  public:
    /// Uses the `legendary` on PATH.
    Provider();

    /// Uses a specific executable, which the tests point at a stub.
    explicit Provider(std::string binary);

    [[nodiscard]] Source source() const override {
        return Source::Epic;
    }

    /// Lists owned titles, installed or not. Throws when Legendary is missing or has no saved
    /// credentials, which is an ordinary state the catalog reports rather than a
    /// failure to start with.
    [[nodiscard]] std::vector<Game> list() override;

  private:
    /// Legendary's stdout for `arguments`. Throws as `list` does.
    [[nodiscard]] std::string run(const std::vector<std::string>& arguments) const;

    std::string binary_;
};

} // namespace iideck::library::epic
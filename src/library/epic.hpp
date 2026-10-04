// epic — lists Epic Games Store titles from the local Legendary installation.
//
// Legendary owns authentication and downloads; this only reads what it has
// already installed, so the grid can show those titles and launch them the same
// way.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "game.hpp"

namespace iideck::library::epic {

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

    /// Lists installed titles. Throws when Legendary is missing or has no saved
    /// credentials, which is an ordinary state the catalog reports rather than a
    /// failure to start with.
    [[nodiscard]] std::vector<Game> list() override;

  private:
    std::string binary_;
};

} // namespace iideck::library::epic
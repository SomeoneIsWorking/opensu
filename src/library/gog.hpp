// gog — lists GOG titles from the local Heroic Games Launcher installation.
//
// Heroic owns authentication, downloads and cloud sync; this only reads its
// cached library and hands launches back to it.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "game.hpp"

namespace iideck::library::gog {

/// Reads Heroic's cached GOG library.
class Provider final : public library::Provider {
  public:
    /// Uses Heroic's default configuration directory.
    Provider();

    /// Uses an explicit configuration directory and executable, which the tests
    /// point at a fixture.
    Provider(std::filesystem::path configDir, std::string binary);

    [[nodiscard]] Source source() const override {
        return Source::Gog;
    }

    /// Lists titles. Throws when Heroic's configuration directory is absent, and
    /// returns nothing when it holds no saved library yet.
    [[nodiscard]] std::vector<Game> list() override;

  private:
    std::filesystem::path configDir_;
    std::string binary_;
};

} // namespace iideck::library::gog
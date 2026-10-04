// roms — scans directories of emulator ROMs and resolves a per-system emulator
// command for each one.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "game.hpp"

namespace iideck::library::roms {

/// Scans configured ROM directories.
class Provider final : public library::Provider {
  public:
    Provider(std::vector<std::filesystem::path> roots,
             std::map<std::string, std::string, std::less<>> extensions,
             std::map<std::string, std::vector<std::string>, std::less<>> emulators);

    [[nodiscard]] Source source() const override {
        return Source::Rom;
    }

    /// Lists every ROM under the configured roots. Returns nothing when no roots
    /// are configured, and reports a root that does not exist rather than
    /// ignoring it.
    [[nodiscard]] std::vector<Game> list() override;

    /// The configured roots.
    [[nodiscard]] const std::vector<std::filesystem::path>& roots() const noexcept {
        return roots_;
    }

    /// The systems the configured extensions name, sorted.
    [[nodiscard]] std::vector<std::string> systems() const;

  private:
    std::vector<std::filesystem::path> roots_;
    std::map<std::string, std::string, std::less<>> extensions_;
    std::map<std::string, std::vector<std::string>, std::less<>> emulators_;
};

} // namespace iideck::library::roms
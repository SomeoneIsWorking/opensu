// A macOS `.app` bundle as its Info.plist describes it: the program it starts, its name and its
// icon file. The one reader of Info.plist.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace opensu::library {

class MacBundle {
  public:
    /// The bundle at `app`, its Info.plist read; one without a readable XML plist answers from its
    /// folder name alone.
    explicit MacBundle(std::filesystem::path app);

    /// The `.app` bundle `program` sits in; empty when it is in none.
    [[nodiscard]] static std::filesystem::path containing(const std::filesystem::path& program);

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }
    /// The string the plist gives `key`; empty when it gives none.
    [[nodiscard]] std::string value(std::string_view key) const;
    /// The program it starts: `Contents/MacOS/<CFBundleExecutable>`, else the bundle's name there.
    [[nodiscard]] std::filesystem::path executable() const;
    /// Its display name, else its name, else the folder's.
    [[nodiscard]] std::string title() const;
    /// The `.icns` the plist names under `Contents/Resources`; empty when it names none.
    [[nodiscard]] std::filesystem::path iconFile() const;

  private:
    std::filesystem::path root_;
    std::string plist_;
};

} // namespace opensu::library

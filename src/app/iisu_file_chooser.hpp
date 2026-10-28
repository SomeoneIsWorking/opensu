// iisu_file_chooser — the file chooser the player gives openSU iiSU's release APK or starter pack
// on, for the console art and the interface sounds alike: it opens in Downloads (or beside the
// file chosen before), lists APKs and zips and refuses, with why, a file IisuPack will not read or
// one that lacks what was asked for.
#pragma once

#include <filesystem>
#include <functional>

#include "path_editor.hpp"

namespace opensu::app {

class IisuFileChooser {
  public:
    /// What the file must hold.
    enum class Need : std::uint8_t {
        /// Console cards: the release APK or the starter pack.
        Art,
        /// The interface sounds: the release APK only.
        Sounds,
    };

    /// The chooser opens in `home`'s Downloads, else `home`.
    IisuFileChooser(PathEditor& paths, std::filesystem::path home)
        : paths_{paths}, home_{std::move(home)} {
    }

    /// Opens the chooser; `known` is the file chosen before, or empty. `chosen` takes a file
    /// accepted.
    void choose(Need need, const std::filesystem::path& known,
                std::function<void(const std::filesystem::path&)> chosen);

  private:
    PathEditor& paths_;
    std::filesystem::path home_;
};

} // namespace opensu::app

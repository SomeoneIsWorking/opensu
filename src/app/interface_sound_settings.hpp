// interface_sound_settings — Settings › Audio › Interface sounds: openSU's own (the default) or
// iiSU's, read from the iiSU release the player already gave for the console art, else from one
// picked here on the same chooser and kept as the same file.
#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "iisu_file_chooser.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class InterfaceSoundSettings {
  public:
    struct Hooks {
        /// The choice changed: the sounds are loaded again.
        std::function<void()> changed;
        /// The iiSU file changed, which the console art reads too.
        std::function<void()> fileChanged;
    };

    InterfaceSoundSettings(Preferences& preferences, IisuFileChooser& chooser, Hooks hooks)
        : preferences_{preferences}, chooser_{chooser}, hooks_{std::move(hooks)} {
    }

    /// The Audio page's row.
    [[nodiscard]] ui::SettingsRow row() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Either step lands on the other choice; iiSU's asks for a release first when the file kept
    /// has no sounds.
    void run(const std::string& id, int step);

  private:
    void use(settings::AssetOwner owner, const std::filesystem::path& iisuFile);

    Preferences& preferences_;
    IisuFileChooser& chooser_;
    Hooks hooks_;
};

} // namespace opensu::app

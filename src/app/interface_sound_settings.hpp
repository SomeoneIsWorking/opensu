// interface_sound_settings — Settings › Audio › Interface sounds: the interface theme's (the
// default), one of openSU's sets (Marimba, Glass, Pluck) or iiSU's, offered only while iiSU's
// release APK is placed in the iiSU folder; and Settings › Audio › Menu music, the theme's loop.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "artwork/iisu_install.hpp"
#include "interface_theme_settings.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class InterfaceSoundSettings {
  public:
    /// `themes` holds the theme in force; `changed` runs when a choice changed: the sounds are
    /// loaded again.
    InterfaceSoundSettings(Preferences& preferences, const artwork::IisuInstall& iisu,
                           InterfaceThemeSettings& themes, std::function<void()> changed)
        : preferences_{preferences}, iisu_{iisu}, themes_{themes}, changed_{std::move(changed)} {
    }

    /// The Audio page's rows: the interface sounds, then the menu music.
    [[nodiscard]] ui::SettingsRow row() const;
    [[nodiscard]] ui::SettingsRow musicRow() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps the interface sounds through the offered choices, wrapping, A stepping forward; or
    /// turns the menu music on or off.
    void run(const std::string& id, int step);

  private:
    /// The iiSU file placed, when it holds the sounds.
    [[nodiscard]] std::shared_ptr<const artwork::IisuPack> iisuSounds() const;
    /// The theme's (none chosen), openSU's sets, then iiSU's while it is placed.
    [[nodiscard]] std::vector<std::optional<settings::SoundSet>> offered() const;
    /// The choice the row shows: the player's, or the openSU set playing in its place while
    /// iiSU's is chosen but gone.
    [[nodiscard]] std::optional<settings::SoundSet> playing() const;

    Preferences& preferences_;
    const artwork::IisuInstall& iisu_;
    InterfaceThemeSettings& themes_;
    std::function<void()> changed_;
};

} // namespace opensu::app

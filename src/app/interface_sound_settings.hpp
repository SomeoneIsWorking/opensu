// interface_sound_settings — Settings › Audio › Interface sounds: one of openSU's sets (Marimba,
// the default, Glass, Pluck) or iiSU's, offered only while iiSU's release APK is placed in the
// iiSU folder.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "artwork/iisu_install.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class InterfaceSoundSettings {
  public:
    /// `changed` runs when the choice changed: the sounds are loaded again.
    InterfaceSoundSettings(Preferences& preferences, const artwork::IisuInstall& iisu,
                           std::function<void()> changed)
        : preferences_{preferences}, iisu_{iisu}, changed_{std::move(changed)} {
    }

    /// The Audio page's row.
    [[nodiscard]] ui::SettingsRow row() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps through the offered sets, wrapping; A steps forward.
    void run(const std::string& id, int step);

  private:
    /// The iiSU file placed, when it holds the sounds.
    [[nodiscard]] std::shared_ptr<const artwork::IisuPack> iisuSounds() const;
    /// openSU's sets, then iiSU's while it is placed.
    [[nodiscard]] std::vector<settings::SoundSet> offered() const;
    /// The set the row shows: the choice, or the openSU set playing in its place while iiSU's is
    /// chosen but gone.
    [[nodiscard]] settings::SoundSet playing() const;

    Preferences& preferences_;
    const artwork::IisuInstall& iisu_;
    std::function<void()> changed_;
};

} // namespace opensu::app

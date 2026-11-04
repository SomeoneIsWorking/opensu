// save_settings — the Settings screen's Saves category: where saves are shared beside the LAN
// (off, a folder through `PathEditor`, or a WebDAV share's address, user and password), and the
// passphrase every openSU sharing it seals the saves with. The password and the passphrase go to
// the secret store, never the settings file; each change is handed to `SaveSharing::share`.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "keyboard_pad.hpp"
#include "path_editor.hpp"
#include "preferences.hpp"
#include "save_sharing.hpp"
#include "secrets/secret_store.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class SaveSettings {
  public:
    /// After a change: the screen shows it.
    using Changed = std::function<void()>;

    struct Services {
        SaveSharing& sharing;
        Preferences& preferences;
        PathEditor& paths;
        gpu2d::ui::Navigator& navigator;
        KeyboardPad& keyboard;
        audio::SdlSoundPlayer& sounds;
    };

    SaveSettings(Services services, secrets::SecretStore secrets, Changed changed);

    static constexpr std::string_view categoryId = "saves";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    void run(const std::string& id, int step);

  private:
    /// One field's entry card: `secretName` keeps a secret, else `field` is the setting.
    struct Field {
        std::string title;
        std::string note;
        std::string label;
        bool secret{false};
        std::string secretName;
        std::string settings::Settings::* field{nullptr};
    };
    void ask(Field field);
    void closeEntry();
    /// Keeps the settings and shares again.
    void apply();
    [[nodiscard]] bool hasSecret(std::string_view name) const;

    Services services_;
    secrets::SecretStore secrets_;
    Changed changed_;
    std::string draft_;
};

} // namespace opensu::app

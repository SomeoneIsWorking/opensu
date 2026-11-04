#include "save_settings.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "gpu2d/ui/screen.hpp"

#include "lucent/log.h"
#include "secrets/secret.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view kindId = "saves.share";
constexpr std::string_view folderId = "saves.folder";
constexpr std::string_view urlId = "saves.url";
constexpr std::string_view userId = "saves.user";
constexpr std::string_view passwordId = "saves.password";
constexpr std::string_view passphraseId = "saves.passphrase";
constexpr std::string_view entryId = "saves-entry";
/// A path, an address or a passphrase fits in this.
constexpr std::size_t longestField = 1024;

} // namespace

SaveSettings::SaveSettings(Services services, secrets::SecretStore secrets, Changed changed)
    : services_{services}, secrets_{std::move(secrets)}, changed_{std::move(changed)} {
}

bool SaveSettings::hasSecret(std::string_view name) const {
    std::string error;
    std::optional<secrets::SecretStore::Found> found = secrets_.read(name, error);
    if (found) {
        secrets::wipe(found->value);
    }
    return found.has_value();
}

ui::SettingsCategory SaveSettings::page() const {
    const settings::Settings& v = services_.preferences.values();
    ui::SettingsCategory page{std::string{categoryId}, "Saves", {}};
    ui::Options kinds;
    for (const settings::SaveShareKind kind : settings::allSaveShareKinds) {
        kinds.labels.emplace_back(settings::label(kind));
    }
    kinds.chosen = static_cast<std::size_t>(v.saveShare);
    page.rows.push_back(ui::makeChoice(std::string{kindId}, "Share saves",
                                       "Beside the LAN: a synced folder or a WebDAV share every "
                                       "openSU of yours reaches",
                                       std::move(kinds)));
    if (v.saveShare == settings::SaveShareKind::Off) {
        return page;
    }
    if (v.saveShare == settings::SaveShareKind::Folder) {
        page.rows.push_back(ui::makeRow(std::string{folderId}, RowKind::Action, "Folder",
                                        "Saves go under opensu-saves in it, sealed",
                                        v.saveShareFolder.empty() ? std::string{"Choose"}
                                                                  : v.saveShareFolder.string()));
    } else {
        page.rows.push_back(ui::makeRow(std::string{urlId}, RowKind::Action, "Address",
                                        "The WebDAV folder, https://…",
                                        v.saveShareUrl.empty() ? "Set" : v.saveShareUrl));
        page.rows.push_back(ui::makeRow(std::string{userId}, RowKind::Action, "User", "",
                                        v.saveShareUser.empty() ? "Set" : v.saveShareUser));
        page.rows.push_back(ui::makeRow(std::string{passwordId}, RowKind::Action, "Password",
                                        "Kept in the keyring",
                                        hasSecret(webDavPasswordSecret) ? "Change" : "Set"));
    }
    page.rows.push_back(
        ui::makeRow(std::string{passphraseId}, RowKind::Action, "Passphrase",
                    "Seals the saves there; the same on every openSU that shares them",
                    hasSecret(sharePassphraseSecret) ? "Change" : "Set"));
    return page;
}

bool SaveSettings::owns(const std::string& id) {
    return id.starts_with(std::string{categoryId} + ".");
}

void SaveSettings::apply() {
    services_.preferences.save();
    services_.sharing.share(services_.preferences.values());
    changed_();
}

void SaveSettings::run(const std::string& id, int step) {
    settings::Settings& v = services_.preferences.values();
    if (id == kindId) {
        v.saveShare =
            ui::stepped(std::vector<settings::SaveShareKind>{settings::allSaveShareKinds.begin(),
                                                             settings::allSaveShareKinds.end()},
                        v.saveShare, step);
        apply();
        return;
    }
    if (step != 0) {
        return;
    }
    if (id == folderId) {
        services_.paths.begin(
            PathEditor::Request{.title = "Saves folder",
                                .start = v.saveShareFolder,
                                .clearLabel = {},
                                .refusal = {},
                                .files = {},
                                .done = [this](std::optional<std::filesystem::path> chosen) {
                                    if (chosen) {
                                        services_.preferences.values().saveShareFolder = *chosen;
                                        apply();
                                    }
                                }});
        return;
    }
    if (id == urlId) {
        ask(Field{.title = "WebDAV address",
                  .note = "The folder's address, as your server or NAS gives it.",
                  .label = "Address",
                  .secret = false,
                  .secretName = {},
                  .field = &settings::Settings::saveShareUrl});
    } else if (id == userId) {
        ask(Field{.title = "WebDAV user",
                  .note = {},
                  .label = "User",
                  .secret = false,
                  .secretName = {},
                  .field = &settings::Settings::saveShareUser});
    } else if (id == passwordId) {
        ask(Field{.title = "WebDAV password",
                  .note = "Kept in the keyring, never in the settings file.",
                  .label = "Password",
                  .secret = true,
                  .secretName = std::string{webDavPasswordSecret},
                  .field = nullptr});
    } else if (id == passphraseId) {
        ask(Field{.title = "Saves passphrase",
                  .note = "The saves are sealed with it where they are shared. Use the same one on "
                          "every openSU; a lost one cannot be recovered.",
                  .label = "Passphrase",
                  .secret = true,
                  .secretName = std::string{sharePassphraseSecret},
                  .field = nullptr});
    }
}

void SaveSettings::ask(Field field) {
    draft_ = field.field != nullptr ? services_.preferences.values().*field.field : std::string{};
    services_.sounds.play(audio::Effect::Open);
    auto shared = std::make_shared<const Field>(std::move(field));
    services_.navigator.push(std::make_unique<ui::EntryScreen>(ui::Entry{
        .id = std::string{entryId},
        .title = shared->title,
        .note = shared->note,
        .label = shared->label,
        .placeholder = {},
        .secret = shared->secret,
        .mostBytes = longestField,
        .text = [this]() -> const std::string& {
            return draft_;
        },
        .changed =
            [this](std::string text) {
                draft_ = std::move(text);
            },
        .submitted =
            [this, shared] {
                closeEntry();
                if (shared->field != nullptr) {
                    services_.preferences.values().*shared->field = draft_;
                } else if (std::string error;
                           !secrets_.save(shared->secretName, shared->title, draft_, error)) {
                    lucent::warn("saves", "{} cannot be kept: {}", shared->title, error);
                }
                secrets::wipe(draft_);
                draft_.clear();
                apply();
            },
        .cancelled =
            [this] {
                closeEntry();
                secrets::wipe(draft_);
                draft_.clear();
            }}));
    services_.keyboard.open();
}

void SaveSettings::closeEntry() {
    for (const gpu2d::ui::Screen* screen : services_.navigator.screens()) {
        if (screen->id() == entryId) {
            services_.sounds.play(audio::Effect::Close);
            services_.navigator.closeFrom(*screen);
            return;
        }
    }
}

} // namespace opensu::app

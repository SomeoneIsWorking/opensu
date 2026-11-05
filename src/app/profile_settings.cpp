#include "profile_settings.hpp"

#include <memory>
#include <vector>

#include "gpu2d/ui/screen.hpp"

#include "lan/peer_book.hpp"
#include "ui/entry_screen.hpp"

namespace opensu::app {
namespace {

using ui::RowKind;

constexpr std::string_view addId = "profiles.add";
constexpr std::string_view namePrefix = "profiles.name.";
constexpr std::string_view picturePrefix = "profiles.picture.";
constexpr std::string_view playPrefix = "profiles.play.";
constexpr std::string_view nameCardId = "profile-name";

std::string rowOf(std::string_view prefix, const std::string& profileId) {
    return std::string{prefix} + profileId;
}

} // namespace

ui::SettingsCategory ProfileSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Profiles", {}};
    const std::string active = profiles_.active().id;
    for (const profiles::Profile& profile : profiles_.all()) {
        page.rows.push_back(ui::makeHeading(profile.name));
        page.rows.push_back(ui::makeRow(rowOf(namePrefix, profile.id), RowKind::Action, "Name",
                                        "How this profile is shown here and on the LAN",
                                        profile.name));
        ui::Options pictures{.labels = {}, .chosen = profile.picture};
        for (std::uint8_t picture = 0; picture < profiles::pictureCount; ++picture) {
            pictures.labels.emplace_back(profiles::pictureName(picture));
        }
        page.rows.push_back(ui::makeChoice(rowOf(picturePrefix, profile.id), "Picture",
                                           "The colour it is shown in", std::move(pictures)));
        if (profile.id == active) {
            page.rows.push_back(ui::makeRow(rowOf(playPrefix, profile.id), RowKind::Info,
                                            "Playing now", "Games load and keep its saves", ""));
        } else {
            page.rows.push_back(ui::makeRow(rowOf(playPrefix, profile.id), RowKind::Action,
                                            "Play as " + profile.name,
                                            "Games load and keep its saves from now on", ""));
        }
    }
    page.rows.push_back(ui::makeHeading("New"));
    page.rows.push_back(ui::makeRow(std::string{addId}, RowKind::Action, "Add profile",
                                    "Another player, with saves of their own", ""));
    return page;
}

bool ProfileSettings::owns(const std::string& id) {
    return id == addId || id.starts_with(namePrefix) || id.starts_with(picturePrefix) ||
           id.starts_with(playPrefix);
}

void ProfileSettings::run(const std::string& id, int step) {
    if (id.starts_with(picturePrefix)) {
        const std::string profileId = id.substr(picturePrefix.size());
        const std::optional<profiles::Profile> profile = profiles_.find(profileId);
        if (!profile || step == 0) {
            return;
        }
        const int count = profiles::pictureCount;
        const auto next =
            static_cast<std::uint8_t>(((profile->picture + step) % count + count) % count);
        std::string error;
        if (!profiles_.setPicture(profileId, next, error)) {
            hooks_.say("Cannot change the picture: " + error, true);
            return;
        }
        hooks_.changed();
        return;
    }
    if (step != 0) {
        return;
    }
    if (id == addId) {
        askName({});
        return;
    }
    if (id.starts_with(namePrefix)) {
        askName(id.substr(namePrefix.size()));
        return;
    }
    if (id.starts_with(playPrefix)) {
        hooks_.play(id.substr(playPrefix.size()));
    }
}

void ProfileSettings::askName(std::string profileId) {
    const std::optional<profiles::Profile> named = profiles_.find(profileId);
    if (!profileId.empty() && !named) {
        return;
    }
    naming_ = std::move(profileId);
    draft_ = named ? named->name : std::string{};
    sounds_.play(audio::Effect::Open);
    navigator_.push(std::make_unique<ui::EntryScreen>(
        ui::Entry{.id = std::string{nameCardId},
                  .title = named ? "Rename " + named->name : std::string{"Add a profile"},
                  .note = "A player's name. Their saves are kept apart from the other profiles'.",
                  .label = "Name",
                  .placeholder = "Player " + std::to_string(profiles_.all().size() + 1),
                  .secret = false,
                  .mostBytes = lan::longestName,
                  .text = [this]() -> const std::string& {
                      return draft_;
                  },
                  .changed =
                      [this](std::string text) {
                          draft_ = std::move(text);
                      },
                  .submitted =
                      [this] {
                          submitName();
                      },
                  .cancelled =
                      [this] {
                          closeName();
                      }}));
    keyboard_.open();
}

void ProfileSettings::submitName() {
    closeName();
    std::string name = lan::shownName(draft_);
    if (name.empty()) {
        name = "Player " + std::to_string(profiles_.all().size() + 1);
    }
    std::string error;
    if (naming_.empty()) {
        if (!profiles_.add(name, error)) {
            hooks_.say("Cannot add " + name + ": " + error, true);
            return;
        }
        hooks_.say("Added " + name, false);
    } else if (!profiles_.rename(naming_, name, error)) {
        hooks_.say("Cannot rename the profile: " + error, true);
        return;
    }
    hooks_.changed();
}

void ProfileSettings::closeName() {
    for (const gpu2d::ui::Screen* screen : navigator_.screens()) {
        if (screen->id() == nameCardId) {
            sounds_.play(audio::Effect::Close);
            navigator_.closeFrom(*screen);
            return;
        }
    }
}

} // namespace opensu::app

#include "plugin_settings.hpp"

#include <algorithm>

#include "keyboard_walk.hpp"

namespace opensu::app {
namespace {

using plugin::PluginInfo;
using ui::RowKind;

constexpr std::string_view rowPrefix = "plugins.";
constexpr std::string_view accessSuffix = ".access";

std::string rowId(const PluginInfo& info) {
    return std::string{rowPrefix} + info.manifest.id;
}

std::string countOf(std::size_t count, const char* what) {
    return std::to_string(count) + " " + what + (count == 1 ? "" : "s");
}

/// How the plugin is doing, for its switch's note.
std::string stateOf(const PluginInfo& info, bool consent) {
    switch (info.state) {
    case PluginInfo::State::Off:
        return consent ? "Off; turning it on asks you to accept what it can use" : "Off";
    case PluginInfo::State::Loading:
        return "Starting";
    case PluginInfo::State::Stopped:
        return "Stopped: " + info.problem;
    case PluginInfo::State::Refused:
        return "Cannot run: " + info.problem;
    case PluginInfo::State::Running:
        break;
    }
    if (info.faults > 0) {
        return "Fault " + std::to_string(info.faults) + " of " +
               std::to_string(plugin::faultLimit) + ": " + info.problem;
    }
    std::string text =
        info.reported ? "Play time for " + countOf(*info.reported, "game") : std::string{"Running"};
    if (info.listed) {
        text += "; emulator lists " + countOf(*info.listed, "game") + ", " +
                std::to_string(info.matched) + " matched";
    }
    return text;
}

} // namespace

std::string PluginSettings::shown(const std::filesystem::path& path) const {
    std::string text = path.string();
    const std::string home = home_.string();
    if (!home.empty() && text.starts_with(home + "/")) {
        return "~" + text.substr(home.size());
    }
    return text;
}

ui::SettingsCategory PluginSettings::page() const {
    ui::SettingsCategory page{std::string{categoryId}, "Plugins", {}};
    for (const PluginInfo& info : plugins_.plugins()) {
        const std::string origin = info.bundled ? "Bundled" : "Added by you";
        if (info.state == PluginInfo::State::Refused) {
            page.rows.push_back(ui::makeRow(rowId(info), RowKind::Info, info.manifest.name,
                                            origin + "; " + stateOf(info, false), "Cannot run"));
            continue;
        }
        page.rows.push_back(ui::makeToggle(rowId(info), info.manifest.name,
                                           origin + " · " + info.manifest.version + " · " +
                                               stateOf(info, plugins_.needsConsent(info)),
                                           plugins_.enabled(info)));
        std::string access;
        for (const std::filesystem::path& path : info.readPaths) {
            access += (access.empty() ? "" : ", ") + shown(path);
        }
        std::string hosts;
        for (const std::string& host : info.hosts) {
            hosts += (hosts.empty() ? "" : ", ") + host;
        }
        page.rows.push_back(ui::makeRow(rowId(info) + std::string{accessSuffix}, RowKind::Info,
                                        "What it can use",
                                        hosts.empty() ? "No network" : "Connects to " + hosts,
                                        access.empty() ? "Reads no files" : "Reads " + access));
    }
    if (page.rows.empty()) {
        page.rows.push_back(ui::makeRow("plugins.none", RowKind::Info, "No plugins",
                                        "Plugins you add go in the data folder's plugins folder",
                                        ""));
    }
    return page;
}

bool PluginSettings::owns(const std::string& id) {
    return id.starts_with(rowPrefix);
}

void PluginSettings::run(const std::string& id, int step) {
    if (step != 0) {
        return;
    }
    const auto info = std::ranges::find_if(plugins_.plugins(), [&id](const PluginInfo& it) {
        return rowId(it) == id && it.state != PluginInfo::State::Refused;
    });
    if (info == plugins_.plugins().end()) {
        return;
    }
    if (plugins_.enabled(*info)) {
        plugins_.setEnabled(info->manifest.id, false);
    } else if (plugins_.needsConsent(*info)) {
        ask(*info);
    } else {
        plugins_.setEnabled(info->manifest.id, true);
    }
}

void PluginSettings::ask(const PluginInfo& info) {
    ui::SessionDialogText text;
    text.title = "Turn on " + info.manifest.name + "?";
    text.paragraphs.push_back(info.manifest.name + " " + info.manifest.version +
                              " is a plugin you added, from " + shown(info.dir) +
                              ". It runs in a sandbox and can use only what is listed here.");
    if (info.readPaths.empty()) {
        text.paragraphs.emplace_back("It reads no files.");
    }
    for (const std::filesystem::path& path : info.readPaths) {
        text.paragraphs.push_back("It can read what is in " + shown(path) + ".");
    }
    if (info.hosts.empty()) {
        text.paragraphs.emplace_back("It does not use the network.");
    }
    for (const std::string& host : info.hosts) {
        text.paragraphs.push_back("It can download from " + host + " over HTTPS.");
    }
    text.paragraphs.emplace_back("Turn it on only if you trust where it came from.");
    text.accept = "Turn on";
    asking_ = info.manifest.id;
    // input-sound.md 3.4 Open: a panel appears.
    sounds_.play(audio::Effect::Open);
    dialog_.open(std::move(text));
}

void PluginSettings::close() {
    // input-sound.md 3.4 Close: the player dismisses a panel.
    sounds_.play(audio::Effect::Close);
    dialog_.close();
    asking_.reset();
    changed_();
}

void PluginSettings::act(gamepad::Button button) {
    switch (button) {
    case gamepad::Button::Left:
    case gamepad::Button::Right:
    case gamepad::Button::Up:
    case gamepad::Button::Down:
        if (dialog_.move(directionOf(button))) {
            sounds_.play(audio::Effect::Navigation);
        }
        break;
    case gamepad::Button::A:
        if (dialog_.focus() == ui::dialogAccept && asking_) {
            plugins_.setEnabled(*asking_, true);
        }
        close();
        break;
    case gamepad::Button::B:
    case gamepad::Button::Start:
        close();
        break;
    default:
        break;
    }
}

} // namespace opensu::app

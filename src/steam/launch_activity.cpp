#include "launch_activity.hpp"

#include <cctype>
#include <string>
#include <string_view>

#include "steam_json.hpp"

namespace opensu::steam {
namespace {

using nlohmann::json;

/// True for `%1$s` / `%2$d`: a positional parameter Steam fills in when it formats the line.
bool isParameter(std::string_view text, std::size_t at, std::size_t& end) noexcept {
    if (text[at] != '%' || at + 2 >= text.size() ||
        !std::isdigit(static_cast<unsigned char>(text[at + 1]))) {
        return false;
    }
    std::size_t j = at + 1;
    while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
        ++j;
    }
    if (j >= text.size() || text[j] != '$') {
        return false;
    }
    ++j;
    if (j >= text.size() || !std::isalpha(static_cast<unsigned char>(text[j]))) {
        return false;
    }
    end = j + 1;
    return true;
}

/// Steam's line without the `%1$s` parameters it has not filled in.
std::string withoutParameters(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t end = 0;
        if (isParameter(text, i, end)) {
            i = end;
            continue;
        }
        if (text[i] == '(') {
            const std::size_t close = text.find(')', i);
            if (close != std::string_view::npos) {
                bool parameter = false;
                for (std::size_t j = i + 1; j < close; ++j) {
                    std::size_t stop = 0;
                    if (isParameter(text, j, stop)) {
                        parameter = true;
                        break;
                    }
                }
                if (parameter) {
                    i = close + 1;
                    continue;
                }
            }
        }
        out.push_back(text[i++]);
    }
    // A removed group leaves the space before it, and a bare `%` from "(%1$s%)".
    while (!out.empty() && (out.back() == ' ' || out.back() == '%')) {
        out.pop_back();
    }
    return out;
}

// Kept on window so it survives between reads; a new SharedJSContext starts without it.
constexpr std::string_view recordAndRead = R"js((() => {
  if (!window.__opensuLaunch) {
    const record = { actions: {}, running: {} };
    const text = key => { try { return LocalizationManager.LocalizeString(key) || ""; }
                          catch (e) { return ""; } };
    const byId = id => Object.values(record.actions).find(a => a.id === id);
    const apps = SteamClient.Apps;
    apps.RegisterForGameActionStart((id, app, name) => {
      record.actions[String(app)] = { id, name, task: "", taskText: "", error: "", ended: false };
    });
    apps.RegisterForGameActionTaskChange((id, app, name, task) => {
      const action = byId(id);
      if (action) { action.task = task; action.taskText = text("#LaunchApp_Action_" + task); }
    });
    apps.RegisterForGameActionShowError((id, app, name, code) => {
      const action = byId(id);
      if (action) {
        const number = String(code).replace("AppError_", "");
        action.error = text("#Steam_AppUpdateError_" + number) || String(code);
      }
    });
    apps.RegisterForGameActionEnd(id => { const action = byId(id); if (action) action.ended = true; });
    SteamClient.GameSessions.RegisterForAppLifetimeNotifications(note => {
      record.running[String(note.unAppID)] = note.bRunning;
    });
    window.__opensuLaunch = record;
  }
  return window.__opensuLaunch;
})())js";

/// Tasks Steam has no localized line for.
std::string taskLine(const json& action) {
    std::string localized = withoutParameters(action.value("taskText", ""));
    if (!localized.empty()) {
        return localized;
    }
    const std::string task = action.value("task", "");
    if (task == "ProcessingInstallScript") {
        return "Running first-time setup";
    }
    return {};
}

} // namespace

Activities parseActivities(const json& record) {
    Activities activities;
    if (!record.is_object()) {
        return activities;
    }
    if (record.contains("actions") && record["actions"].is_object()) {
        for (const auto& [app, action] : record["actions"].items()) {
            launch::SteamAppActivity& activity = activities[app];
            activity.actionId = action.value("id", 0);
            activity.task = taskLine(action);
            activity.error = action.value("error", "");
            activity.actionEnded = action.value("ended", true);
        }
    }
    if (record.contains("running") && record["running"].is_object()) {
        for (const auto& [app, running] : record["running"].items()) {
            if (running.is_boolean()) {
                activities[app].running = running.get<bool>();
            }
        }
    }
    return activities;
}

LaunchActivity::LaunchActivity(DevTools& devTools) : devTools_{devTools} {
}

std::optional<Activities> LaunchActivity::read(std::string& error) {
    const std::optional<json> record = devTools_.evaluate(recordAndRead, error);
    if (!record) {
        return std::nullopt;
    }
    return parseActivities(*record);
}

bool LaunchActivity::cancel(std::string_view appId, std::string& error) {
    if (!isAppId(appId)) {
        error = "not a Steam app id: " + std::string{appId};
        return false;
    }
    // The library passes the game id, which for a Steam app is its app id as a string.
    return devTools_
        .evaluate("SteamClient.Apps.CancelLaunch(\"" + std::string{appId} + "\")", error)
        .has_value();
}

} // namespace opensu::steam

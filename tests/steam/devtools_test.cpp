// The DevTools link, the download queue and the installer, against a fake Steam that serves
// the DevTools page list and answers Runtime.evaluate like the SharedJSContext.
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "devtools.hpp"
#include "downloads.hpp"
#include "install_wizard.hpp"
#include "launch_activity.hpp"
#include "libraries.hpp"
#include "lucent/http_client.h"
#include "support/fake_steam.hpp"

namespace {

using nlohmann::json;
using opensu::steam::DevTools;
using opensu::steam::Download;
using opensu::steam::DownloadQueue;
using opensu::steam::InstallStep;
using opensu::steam::InstallWizard;
using opensu::steam::LaunchActivity;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

using opensu::test::FakeSteam;

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void testDownloadsParse() {
    const json answer = json::parse(R"({
      "overview": {"update_appid": 960090, "paused": false, "overall_percent_complete": 53,
                   "overall_estimated_time_remaining_sec": 34},
      "items": [{"remote_client_id": "0", "item_data": [
        {"appid": 960090, "active": false, "paused": false, "completed": false,
         "update_type_info": [{"has_update": true, "completed_update": false,
                               "overall_percent_complete": 40}]},
        {"appid": 292030, "active": false, "paused": true, "completed": false,
         "update_type_info": [{"has_update": false, "completed_update": false,
                               "overall_percent_complete": 0},
                              {"has_update": true, "completed_update": false,
                               "overall_percent_complete": 10}]},
        {"appid": 480, "active": false, "paused": false, "completed": true,
         "update_type_info": [{"has_update": true, "completed_update": true,
                               "overall_percent_complete": 100}]}]},
        {"remote_client_id": "77", "item_data": [
        {"appid": 1, "completed": false,
         "update_type_info": [{"has_update": true, "overall_percent_complete": 5}]}]}]
    })");
    const std::vector<Download> downloads = opensu::steam::parseDownloads(answer);
    expect(downloads.size() == 2, "finished and remote downloads are left out");
    expect(downloads[0].appId == "960090" && downloads[0].active && downloads[0].progress == 0.53 &&
               downloads[0].secondsLeft == 34,
           "the active download takes the overview's live figures");
    expect(downloads[1].appId == "292030" && !downloads[1].active && downloads[1].paused &&
               downloads[1].progress == 0.10 && !downloads[1].secondsLeft,
           "a queued download keeps its own part's progress");
    expect(opensu::steam::parseDownloads(json::object()).empty(), "no answer, no downloads");
}

void testQueueReadsThroughDevTools() {
    FakeSteam steam{[](const std::string& expression) {
        expect(contains(expression, "RegisterForDownloadItems"), "the queue asks for items");
        return json::parse(R"({"overview": {"update_appid": 0},
            "items": [{"remote_client_id": "0", "item_data": [{"appid": 7, "completed": false,
            "update_type_info": [{"has_update": true, "overall_percent_complete": 25}]}]}]})");
    }};
    DevTools devTools{steam.port()};
    DownloadQueue queue{devTools};
    std::string error;
    const std::optional<std::vector<Download>> first = queue.read(error);
    expect(first && first->size() == 1 && (*first)[0].appId == "7" && (*first)[0].progress == 0.25,
           "the queue is read from Steam");
    expect(queue.read(error).has_value() && steam.calls().size() == 2,
           "a second read reuses the connection");
}

void testLaunchActivityParse() {
    const json record = json::parse(R"({
      "actions": {
        "268910": {"id": 5, "name": "LaunchApp", "task": "ProcessingInstallScript",
                   "taskText": "", "error": "", "ended": false},
        "960090": {"id": 3, "name": "LaunchApp", "task": "SynchronizingCloud",
                   "taskText": "Synchronizing cloud", "error": "Game already running",
                   "ended": true}},
      "running": {"960090": true, "480": false}
    })");
    const opensu::steam::Activities activities = opensu::steam::parseActivities(record);
    expect(activities.size() == 3, "every app the record names is read");
    const auto& setup = activities.at("268910");
    expect(setup.actionId == 5 && setup.task == "Running first-time setup" && !setup.actionEnded &&
               !setup.running,
           "the install script gets a line Steam has none for");
    const auto& other = activities.at("960090");
    expect(other.task == "Synchronizing cloud" && other.error == "Game already running" &&
               other.actionEnded && other.running == true,
           "Steam's own words and running state are kept");
    expect(activities.at("480").running == false && activities.at("480").actionId == 0,
           "a running state without an action stands alone");
    expect(opensu::steam::parseActivities(json{}).empty(), "no record, no activity");
}

void testLaunchActivityReadsThroughDevTools() {
    FakeSteam steam{[](const std::string& expression) {
        expect(contains(expression, "RegisterForGameActionStart") &&
                   contains(expression, "RegisterForAppLifetimeNotifications"),
               "the recorder follows actions and app lifetimes");
        return json::parse(R"({"actions": {}, "running": {"7": true}})");
    }};
    DevTools devTools{steam.port()};
    LaunchActivity launches{devTools};
    std::string error;
    const std::optional<opensu::steam::Activities> read = launches.read(error);
    expect(read && read->at("7").running == true, "activity is read from Steam");
}

void testUnreachableSteam() {
    std::uint16_t port = 0;
    {
        const FakeSteam gone{[](const std::string&) {
            return json{};
        }};
        port = gone.port();
    }
    DevTools devTools{port};
    std::string error;
    expect(!devTools.evaluate("1", error) && !error.empty(), "a closed port is reported");
}

void testInstallWalksTheWizard() {
    // Opened, then app info, then config, then a licence, then handed off.
    std::vector<int> states{5, 7, 8, 0};
    std::size_t at = 0;
    FakeSteam steam{[&states, &at](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            const int state = states[std::min(at, states.size() - 1)];
            return {{"state", state}, {"app", state == 0 ? 0 : 480}, {"error", ""}};
        }
        if (contains(expression, "ContinueInstall")) {
            ++at;
            return json{};
        }
        if (contains(expression, "LoadEula")) {
            return json::array({{{"id", "480_eula_0"}, {"url", "http://eula"}, {"version", 2}}});
        }
        if (contains(expression, "OpenInstallWizard")) {
            return json{};
        }
        if (contains(expression, "MarkEulaAccepted")) {
            return json{};
        }
        expect(false, "only installer calls are made");
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};

    expect(wizard.open("480").kind == InstallStep::Kind::Working, "the wizard waits on app info");
    at = 1;
    expect(wizard.poll().kind == InstallStep::Kind::Working, "the config step is continued");
    const InstallStep eula = wizard.poll();
    expect(eula.kind == InstallStep::Kind::NeedsEula && eula.eulas.size() == 1 &&
               eula.eulas[0].id == "480_eula_0" && eula.eulas[0].version == 2,
           "a licence is handed to the player, not accepted");
    expect(wizard.accept(eula.eulas).kind == InstallStep::Kind::Working,
           "an accepted licence continues the install");
    expect(wizard.poll().kind == InstallStep::Kind::Queued, "the handed-off install is queued");

    const std::vector<std::string> calls = steam.calls();
    bool marked = false;
    for (const std::string& call : calls) {
        marked = marked || call == "SteamClient.Apps.MarkEulaAccepted(480,\"480_eula_0\",2)";
    }
    expect(marked, "the accepted licence is recorded with its id and version");
}

void testInstallRefusesWhatOnlySteamCanAsk() {
    FakeSteam steam{[](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 4}, {"app", 480}, {"error", ""}};
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    const InstallStep step = wizard.open("480");
    expect(step.kind == InstallStep::Kind::Failed && !step.failure.empty(),
           "a CD key prompt fails the install");
    expect(contains(steam.calls().back(), "CancelInstall"), "and closes Steam's wizard");
    expect(wizard.open("48;0").kind == InstallStep::Kind::Failed,
           "an app id that is not a number never reaches Steam");
}

/// A fake Steam at the install wizard's config step whose libraries are `libraries`; an
/// AddInstallFolder call appends its path when `adds` is set.
json librariesOf(const std::vector<std::string>& paths) {
    json list = json::array();
    for (std::size_t i = 0; i < paths.size(); ++i) {
        list.push_back({{"nFolderIndex", i}, {"strFolderPath", paths[i]}});
    }
    return list;
}

int indexOfCall(const std::vector<std::string>& calls, const std::string& needle) {
    for (std::size_t i = 0; i < calls.size(); ++i) {
        if (contains(calls[i], needle)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void testInstallChoosesTheLibrary() {
    std::vector<std::string> libraries{"/home/p/.local/share/Steam", "/mnt/games"};
    FakeSteam steam{[&libraries](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 7}, {"app", 480}, {"error", ""}};
        }
        if (contains(expression, "GetInstallFolders")) {
            return librariesOf(libraries);
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    expect(wizard.open("480", std::filesystem::path{"/mnt/games"}).kind ==
               InstallStep::Kind::Working,
           "the config step is continued");
    const std::vector<std::string> calls = steam.calls();
    expect(indexOfCall(calls, "AddInstallFolder") < 0, "a library Steam has is not added again");
    const int set = indexOfCall(calls, "SetInstallFolder(1)");
    expect(set >= 0 && set < indexOfCall(calls, "ContinueInstall"),
           "the install is pointed at Steam's library before it continues");
    expect(wizard.poll().kind == InstallStep::Kind::Working, "a second poll continues");
    expect(indexOfCall(steam.calls(), "SetInstallFolder(1)") == set &&
               steam.calls().size() > calls.size(),
           "the folder is chosen once");
}

void testInstallAddsAMissingLibrary() {
    std::vector<std::string> libraries{"/home/p/.local/share/Steam"};
    FakeSteam steam{[&libraries](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 7}, {"app", 480}, {"error", ""}};
        }
        if (contains(expression, "GetInstallFolders")) {
            return librariesOf(libraries);
        }
        if (contains(expression, "AddInstallFolder")) {
            libraries.emplace_back("/mnt/new");
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    expect(wizard.open("480", std::filesystem::path{"/mnt/new/"}).kind ==
               InstallStep::Kind::Working,
           "a folder Steam lacks is added as a library");
    const std::vector<std::string> calls = steam.calls();
    const int add = indexOfCall(calls, "AddInstallFolder(\"/mnt/new/\")");
    const int set = indexOfCall(calls, "SetInstallFolder(1)");
    expect(add >= 0 && add < set && set < indexOfCall(calls, "ContinueInstall"),
           "it is added, then chosen, then the install continues");
}

void testInstallFailsWhenSteamKeepsNoLibrary() {
    FakeSteam steam{[](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 7}, {"app", 480}, {"error", ""}};
        }
        if (contains(expression, "GetInstallFolders")) {
            return librariesOf({"/home/p/.local/share/Steam"});
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    const InstallStep step = wizard.open("480", std::filesystem::path{"/mnt/new"});
    expect(step.kind == InstallStep::Kind::Failed && contains(step.failure, "/mnt/new"),
           "a library Steam will not keep fails the install, naming the folder");
    expect(indexOfCall(steam.calls(), "ContinueInstall") < 0, "the install never continues");
    expect(contains(steam.calls().back(), "CancelInstall"), "and Steam's wizard is closed");
}

void testInstallWithoutAFolderLeavesSteamsDefault() {
    FakeSteam steam{[](const std::string& expression) -> json {
        if (contains(expression, "GetInstallManagerInfo")) {
            return {{"state", 7}, {"app", 480}, {"error", ""}};
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    InstallWizard wizard{devTools};
    expect(wizard.open("480").kind == InstallStep::Kind::Working, "the config step is continued");
    expect(indexOfCall(steam.calls(), "InstallFolder") < 0 &&
               indexOfCall(steam.calls(), "SetInstallFolder") < 0,
           "with no folder, Steam is not asked about its libraries");
}

/// Steam's GetInstallFolders answer for this machine-like layout: Steam's own library with a game,
/// an empty one (the default), and a second drive with two games.
json installFolders() {
    return json::array({{{"nFolderIndex", 0},
                         {"strFolderPath", "/home/p/.local/share/Steam"},
                         {"strUserLabel", ""},
                         {"nCapacity", "500000000000"},
                         {"nFreeSpace", "100000000000"},
                         {"bIsDefaultFolder", false},
                         {"bIsMounted", true},
                         {"vecApps", json::array({{{"nAppID", 220}, {"nAppSize", "5000"}}})}},
                        {{"nFolderIndex", 1},
                         {"strFolderPath", "/mnt/empty"},
                         {"nCapacity", "1000"},
                         {"nFreeSpace", "900"},
                         {"bIsDefaultFolder", true},
                         {"vecApps", json::array()}},
                        {{"nFolderIndex", 2},
                         {"strFolderPath", "/mnt/games"},
                         {"bIsDefaultFolder", false},
                         {"vecApps", json::array({{{"nAppID", 1}, {"nAppSize", 10}},
                                                  {{"nAppID", 2}, {"nAppSize", 20}}})}}});
}

void testLibrariesThroughDevTools() {
    FakeSteam steam{[](const std::string& expression) -> json {
        if (contains(expression, "GetInstallFolders")) {
            return installFolders();
        }
        return json{};
    }};
    DevTools devTools{steam.port()};
    opensu::steam::Libraries libraries{devTools};
    std::string error;
    const auto read = libraries.read(error);
    expect(read.has_value(), "the libraries are read");
    const std::vector<opensu::steam::SteamLibrary> all =
        read.value_or(std::vector<opensu::steam::SteamLibrary>{});
    expect(all.size() == 3, "every library is listed");
    const auto& own = all[0];
    expect(own.path == "/home/p/.local/share/Steam" && own.capacity == 500000000000ULL &&
               own.freeSpace == 100000000000ULL && own.apps == 1 && own.appBytes == 5000 &&
               !own.isDefault,
           "a library's path, sizes and games are read, strings as numbers");
    expect(all[1].isDefault && all[2].apps == 2 && all[2].appBytes == 30,
           "the default is marked and games summed");

    expect(libraries.add("/mnt/new").empty() &&
               contains(steam.calls().back(), "AddInstallFolder(\"/mnt/new\")"),
           "a library is added by its path");
    expect(libraries.makeDefault(all[2]).empty() &&
               contains(steam.calls().back(), "SetDefaultInstallFolder(2)"),
           "the default is set by Steam's index");
    const std::size_t calls = steam.calls().size();
    const std::string refused = libraries.remove(all[2]);
    expect(contains(refused, "2 games") && steam.calls().size() == calls,
           "a library with games is never removed, and the reason is given");
    expect(contains(libraries.remove(all[0]), "Steam's own") && steam.calls().size() == calls,
           "nor is Steam's own library");
    expect(libraries.remove(all[1]).empty() &&
               contains(steam.calls().back(), "RemoveInstallFolder(1)"),
           "an empty library is removed");
}

} // namespace

int main() {
    try {
        testDownloadsParse();
        testQueueReadsThroughDevTools();
        testLaunchActivityParse();
        testLaunchActivityReadsThroughDevTools();
        testUnreachableSteam();
        testInstallWalksTheWizard();
        testInstallRefusesWhatOnlySteamCanAsk();
        testInstallChoosesTheLibrary();
        testInstallAddsAMissingLibrary();
        testInstallFailsWhenSteamKeepsNoLibrary();
        testInstallWithoutAFolderLeavesSteamsDefault();
        testLibrariesThroughDevTools();
        std::printf("devtools: all checks passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}

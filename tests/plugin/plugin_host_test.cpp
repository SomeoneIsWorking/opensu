// The plugin host through its shipping runtime: the sandbox refuses every way out (io, os, debug,
// loading code or bytecode, require, dofile), the memory and instruction limits stop a runaway
// plugin, three faults in a row stop a plugin while the rest run on, file reads outside the
// manifest's read paths are refused (`..`, symlinks, undeclared paths), HTTP is HTTPS to
// declared hosts only, and JSON round-trips with its depth capped.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "plugin_rig.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using plugin::PluginInfo;
using test::expect;
using test::PluginRig;

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

/// A source that reports nothing, so a plugin that loads registers something.
constexpr const char* quietSource =
    "opensu.register_playtime_source{claims = {source = 'rom', systems = {'switch'}}, "
    "read = function() return {} end}\n";

void expectStoppedWith(const PluginRig& rig, const std::string& id, const std::string& reason,
                       const char* what) {
    const PluginInfo& info = rig.info(id);
    if (info.state != PluginInfo::State::Stopped || !contains(info.problem, reason)) {
        std::fprintf(stderr, "%s: state %d, problem \"%s\"\n", id.c_str(),
                     static_cast<int>(info.state), info.problem.c_str());
        test::fail(what);
    }
}

void sandboxRefusesEveryWayOut(const fs::path& root) {
    PluginRig rig{root / "sandbox"};
    rig.addPlugin("io", std::string{"io.open('/etc/passwd')\n"} + quietSource);
    rig.addPlugin("os", std::string{"os.execute('true')\n"} + quietSource);
    rig.addPlugin("debug", std::string{"debug.getregistry()\n"} + quietSource);
    rig.addPlugin("load", std::string{"load('return 1')\n"} + quietSource);
    rig.addPlugin("dump", std::string{"local f = string.dump(function() end)\n"} + quietSource);
    rig.addPlugin("require", std::string{"require('io')\n"} + quietSource);
    rig.addPlugin("dofile", std::string{"dofile('/etc/passwd')\n"} + quietSource);
    rig.addPlugin("gc", std::string{"collectgarbage('stop')\n"} + quietSource);
    // A precompiled chunk: Lua's signature, then anything.
    rig.addPlugin("bytecode", std::string{"\x1bLua\x54"} + '\0' + "junk");
    rig.addPlugin("memory", std::string{"local bomb = string.rep('x', 1 << 30)\n"} + quietSource);
    rig.addPlugin("loop", std::string{"while true do end\n"} + quietSource);
    rig.addPlugin("fine", std::string{quietSource});
    const plugin::PluginHost::Update update = rig.start();

    expectStoppedWith(rig, "io", "global 'io'", "io is not there");
    expectStoppedWith(rig, "os", "global 'os'", "os is not there");
    expectStoppedWith(rig, "debug", "global 'debug'", "debug is not there");
    expectStoppedWith(rig, "load", "global 'load'", "load is not there");
    expectStoppedWith(rig, "dump", "field 'dump'", "string.dump is not there");
    expectStoppedWith(rig, "require", "global 'require'", "require is not there");
    expectStoppedWith(rig, "dofile", "global 'dofile'", "dofile is not there");
    expectStoppedWith(rig, "gc", "global 'collectgarbage'", "the collector is the host's");
    expectStoppedWith(rig, "bytecode", "binary chunk", "bytecode is never loaded");
    expectStoppedWith(rig, "memory", "not enough memory", "the memory limit stops a string bomb");
    expectStoppedWith(rig, "loop", "ran over the limit of", "the instruction limit stops a loop");
    expect(rig.info("fine").state == PluginInfo::State::Running,
           "a well-behaved plugin runs beside them");
    expect(update.stopped.size() == 11, "each stop is reported once");
}

void threeFaultsInARowStopAPlugin(const fs::path& root) {
    PluginRig rig{root / "faults"};
    // Fails on calls 1, 2, 4, 5 and 6; call 3 succeeds and resets the count.
    rig.addPlugin("flaky", R"(
        local calls = 0
        opensu.register_playtime_source{
          claims = {source = 'rom', systems = {'switch'}},
          read = function(games)
            calls = calls + 1
            if calls ~= 3 then error('broken read ' .. calls) end
            return {[games[1].id] = {seconds = 60}}
          end,
        })");
    rig.addPlugin("steady", R"(
        opensu.register_playtime_source{
          claims = {source = 'rom', systems = {'switch'}},
          read = function(games) return {[games[1].id] = {seconds = 120}} end,
        })");
    rig.start();
    const std::vector<library::Game> games{test::romGame("Game", "/roms/switch/game.nsp")};

    rig.refresh(games);
    expect(rig.info("flaky").state == PluginInfo::State::Running && rig.info("flaky").faults == 1 &&
               contains(rig.info("flaky").problem, "broken read 1"),
           "a fault is counted and shown, and the plugin runs on");
    rig.refresh(games);
    plugin::PluginHost::Update update = rig.refresh(games);
    expect(rig.info("flaky").faults == 0 && update.playTime.at("flaky").begin()->second == 60,
           "a good read resets the count");
    rig.refresh(games);
    rig.refresh(games);
    expect(rig.info("flaky").state == PluginInfo::State::Running && rig.info("flaky").faults == 2,
           "two in a row is not yet the limit");
    update = rig.refresh(games);
    expect(rig.info("flaky").state == PluginInfo::State::Stopped && update.stopped.size() == 1 &&
               update.stopped.front().first == "flaky",
           "the third fault in a row stops it, once");
    update = rig.refresh(games);
    expect(!update.playTime.contains("flaky") && update.playTime.at("steady").size() == 1,
           "a stopped plugin is asked nothing; the others run on");
}

void readsStayInsideTheReadPaths(const fs::path& root) {
    PluginRig rig{root / "permissions"};
    const fs::path allowed = rig.root() / "data" / "allowed";
    test::writeFile(allowed / "inside.txt", "inside");
    test::writeFile(rig.root() / "data" / "secret.txt", "secret");
    fs::create_directory_symlink(rig.root() / "data", allowed / "link");
    // Absolute on every OS, and outside every read path.
    const std::string outside = (rig.root() / "outside.txt").generic_string();
    rig.addPlugin("reader",
                  R"(
        opensu.register_playtime_source{
          claims = {source = 'rom', systems = {'switch'}},
          read = function()
            local root = opensu.paths.read.mine[1]
            local denied = {}
            for _, path in ipairs({')" +
                      outside + R"(', root .. '/../secret.txt',
                                   root .. '/link/secret.txt', 'relative.txt'}) do
              local bytes, err = opensu.fs.read(path)
              denied[#denied + 1] = bytes and ('READ ' .. path) or err
            end
            local inside = opensu.fs.read(root .. '/inside.txt')
            local listed = opensu.fs.list(root)
            local stat = opensu.fs.stat(root .. '/inside.txt')
            local _, plain = opensu.http.get('http://example.com/')
            local _, undeclared = opensu.http.get('https://elsewhere.example/')
            error(table.concat(denied, ' | ') .. ' | ' .. inside .. ' ' .. #listed .. ' ' ..
                  stat.size .. ' | ' .. plain .. ' | ' .. undeclared)
          end,
        })",
                  R"({"mine": ["{xdg_data}/allowed"]})", R"(["declared.example"])");
    rig.start();
    rig.refresh({test::romGame("Game", "/roms/switch/game.nsp")});
    const std::string& problem = rig.info("reader").problem;
    expect(!contains(problem, "READ"), "no file outside the read path was read");
    expect(contains(problem, outside + " is outside the plugin's read paths"),
           "an undeclared path is refused");
    expect(contains(problem, "/../secret.txt is outside"), "climbing out with .. is refused");
    expect(contains(problem, "/link/secret.txt is outside"), "a symlink out is refused");
    expect(contains(problem, "relative.txt is not an absolute path"), "a relative path is refused");
    expect(contains(problem, "inside 2 6"), "reads, lists and stats inside the path work");
    expect(contains(problem, "http://example.com/ is not HTTPS"), "plain HTTP is refused");
    expect(contains(problem, "elsewhere.example is not a host the plugin declared"),
           "an undeclared host is refused");
}

void jsonRoundTripsWithItsDepthCapped(const fs::path& root) {
    PluginRig rig{root / "json"};
    rig.addPlugin("json", R"(
        opensu.register_playtime_source{
          claims = {source = 'rom', systems = {'switch'}},
          read = function(games)
            local value = opensu.json.decode('{"a": [1, 2.5, "x", null, true], "b": {}}')
            assert(value.a[1] == 1 and value.a[2] == 2.5 and value.a[4] == opensu.json.null)
            local again = opensu.json.decode(opensu.json.encode(value))
            assert(again.a[3] == 'x' and again.a[5] == true)
            local deep, err = opensu.json.decode(string.rep('[', 100) .. string.rep(']', 100))
            assert(deep == nil and err:find('deeper'), err)
            local bad, why = opensu.json.encode({f = print or function() end})
            assert(bad == nil and why:find('function'), why)
            assert(opensu.titles.key('Hades  II™') == opensu.titles.key('HADES II'))
            return {[games[1].id] = {seconds = 1}}
          end,
        })");
    rig.start();
    const plugin::PluginHost::Update update =
        rig.refresh({test::romGame("Game", "/roms/switch/game.nsp")});
    expect(rig.info("json").faults == 0 && update.playTime.contains("json"),
           "JSON and title keys behave");
}

void refusedManifestsNeverRun(const fs::path& root) {
    PluginRig rig{root / "manifests"};
    test::writeFile(rig.root() / "user" / "future" / "plugin.json",
                    R"({"id": "future", "name": "F", "version": "1", "api": 2})");
    test::writeFile(rig.root() / "user" / "escape" / "plugin.json",
                    R"({"id": "escape", "name": "E", "version": "1", "api": 1,
                        "permissions": {"read": {"x": ["{home}/../../etc"]}}})");
    test::writeFile(rig.root() / "user" / "broken" / "plugin.json", "{");
    rig.start();
    expect(rig.info("future").state == PluginInfo::State::Refused &&
               contains(rig.info("future").problem, "API 2"),
           "another API version is refused");
    expect(rig.info("escape").state == PluginInfo::State::Refused &&
               contains(rig.info("escape").problem, ".."),
           "a read path that climbs out is refused");
    expect(rig.info("broken").state == PluginInfo::State::Refused, "a broken manifest is refused");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "plugin_host";
    sandboxRefusesEveryWayOut(root);
    threeFaultsInARowStopAPlugin(root);
    readsStayInsideTheReadPaths(root);
    jsonRoundTripsWithItsDepthCapped(root);
    refusedManifestsNeverRun(root);
    fs::remove_all(root);
    std::printf("plugin_host: all checks passed\n");
    return 0;
}

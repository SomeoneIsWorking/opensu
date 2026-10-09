// Finding a process by command line and ending its whole tree, with real processes.
#include "launch/process_tree.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;
using opensu::launch::ProcessTree;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool waitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const Clock::time_point until = Clock::now() + timeout;
    while (Clock::now() < until) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
    return predicate();
}

/// True while the process executes; a zombie has already exited.
bool alive(pid_t pid) {
    std::ifstream in{"/proc/" + std::to_string(pid) + "/stat"};
    std::string text;
    std::getline(in, text);
    const std::size_t close = text.rfind(')');
    return close != std::string::npos && close + 2 < text.size() && text[close + 2] != 'Z';
}

/// Starts `sh -c script marker`, so the marker is the last argument of the shell.
pid_t spawn(const std::string& script, const std::string& marker) {
    const pid_t pid = fork();
    expect(pid >= 0, "fork works");
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", script.c_str(), marker.c_str(), static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    return pid;
}

} // namespace

int main() {
    const std::string id = std::to_string(getpid());
    const std::string nul(1, '\0');
    const std::string hint = "AppId=" + id + nul;

    expect(ProcessTree::matching("").empty(), "an empty hint matches nothing");
    expect(!ProcessTree::anyMatches(hint), "nothing matches before the tree starts");

    // The reaper's shape: the marked shell has children and grandchildren.
    const pid_t root = spawn("sleep 60 & (sleep 61 & sleep 62 & wait) & wait", "AppId=" + id);
    // Marker one digit longer: its argument continues past the hint, so it is not a match.
    const pid_t lookalike = spawn("sleep 63 & wait", "AppId=" + id + "0");
    // A process with no marker, standing in for Steam itself.
    const pid_t bystander = spawn("sleep 64 & wait", "other");

    expect(waitUntil(
               [&] {
                   return ProcessTree::descendants(root).size() >= 4;
               },
               std::chrono::seconds{10}),
           "the tree's descendants are found through parent links");
    const std::vector<pid_t> found = ProcessTree::matching(hint);
    expect(std::ranges::find(found, root) != found.end(), "the marked shell is found");
    expect(std::ranges::find(found, lookalike) == found.end(),
           "a command line that only starts like the hint is not found");

    std::vector<pid_t> tree = ProcessTree::descendants(root);
    tree.push_back(root);
    const std::vector<pid_t> kept = ProcessTree::descendants(lookalike);
    expect(!kept.empty(), "the look-alike has children of its own");
    const std::vector<pid_t> bystanders = ProcessTree::descendants(bystander);

    expect(ProcessTree::killMatching(hint) >= tree.size(),
           "every process of the tree was signalled");
    waitpid(root, nullptr, 0);
    for (const pid_t pid : tree) {
        expect(waitUntil(
                   [pid] {
                       return !alive(pid);
                   },
                   std::chrono::seconds{10}),
               "a process of the killed tree is gone");
    }
    expect(!ProcessTree::anyMatches(hint), "nothing matches once the tree is dead");
    expect(alive(lookalike) && alive(kept.front()), "the look-alike and its child survive");
    expect(alive(bystander), "an unrelated process survives");

    kill(lookalike, SIGKILL);
    kill(bystander, SIGKILL);
    for (const pid_t pid : kept) {
        kill(pid, SIGKILL);
    }
    for (const pid_t pid : bystanders) {
        kill(pid, SIGKILL);
    }
    waitpid(lookalike, nullptr, 0);
    waitpid(bystander, nullptr, 0);

    std::printf("process_tree: all checks passed\n");
    return 0;
}

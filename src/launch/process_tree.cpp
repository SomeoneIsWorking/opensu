#include "process_tree.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>

#include <unistd.h>

#include "lucent/log.h"

namespace opensu::launch {
namespace {

namespace fs = std::filesystem;

/// Rounds of find-and-kill before giving up on a tree that keeps forking.
constexpr int killRounds = 10;
constexpr auto settle = std::chrono::milliseconds{20};

/// Every pid in /proc.
std::vector<ProcessId> allPids() {
    std::vector<ProcessId> pids;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{"/proc", ec}) {
        if (ec) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() < '0' || name.front() > '9') {
            continue;
        }
        pids.push_back(static_cast<ProcessId>(std::stol(name)));
    }
    return pids;
}

/// The parent of `pid`, or -1 when it exited. The command name in /proc/<pid>/stat
/// can hold spaces and parentheses, so the fields are read after its last ')'.
ProcessId parentOf(ProcessId pid) {
    std::ifstream in{"/proc/" + std::to_string(pid) + "/stat"};
    std::string text;
    std::getline(in, text);
    const std::size_t close = text.rfind(')');
    if (close == std::string::npos) {
        return -1;
    }
    std::string state;
    long ppid = -1;
    std::istringstream fields{text.substr(close + 1)};
    if (!(fields >> state >> ppid)) {
        return -1;
    }
    return static_cast<ProcessId>(ppid);
}

} // namespace

std::vector<ProcessId> ProcessTree::matching(const std::string& hint) {
    std::vector<ProcessId> found;
    if (hint.empty()) {
        return found;
    }
    for (const ProcessId pid : allPids()) {
        // The process may exit between the listing and the read, which reads empty.
        std::ifstream cmdline{"/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary};
        const std::string text{std::istreambuf_iterator<char>{cmdline},
                               std::istreambuf_iterator<char>{}};
        if (text.find(hint) != std::string::npos) {
            found.push_back(pid);
        }
    }
    return found;
}

bool ProcessTree::anyMatches(const std::string& hint) {
    return !matching(hint).empty();
}

std::vector<ProcessId> ProcessTree::descendants(ProcessId root) {
    std::multimap<ProcessId, ProcessId> children;
    for (const ProcessId pid : allPids()) {
        if (const ProcessId parent = parentOf(pid); parent >= 0) {
            children.emplace(parent, pid);
        }
    }

    std::vector<ProcessId> found;
    std::vector<ProcessId> frontier{root};
    while (!frontier.empty()) {
        const ProcessId parent = frontier.back();
        frontier.pop_back();
        const auto [first, last] = children.equal_range(parent);
        for (auto it = first; it != last; ++it) {
            if (std::ranges::find(found, it->second) == found.end()) {
                found.push_back(it->second);
                frontier.push_back(it->second);
            }
        }
    }
    return found;
}

std::vector<ProcessId> ProcessTree::treesMatching(const std::string& hint) {
    std::vector<ProcessId> trees;
    for (const ProcessId root : matching(hint)) {
        // opensu is never one of them, whatever it was started with.
        if (root == ownProcessId()) {
            continue;
        }
        const std::vector<ProcessId> below = descendants(root);
        trees.insert(trees.end(), below.rbegin(), below.rend());
        trees.push_back(root);
    }
    return trees;
}

std::size_t ProcessTree::killMatching(const std::string& hint) {
    std::size_t sent = 0;
    for (int round = 0; round < killRounds; ++round) {
        const std::vector<ProcessId> doomed = treesMatching(hint);
        if (doomed.empty()) {
            break;
        }
        for (const ProcessId pid : doomed) {
            if (::kill(pid, SIGKILL) == 0) {
                ++sent;
            }
        }
        std::this_thread::sleep_for(settle);
    }
    if (!treesMatching(hint).empty()) {
        lucent::error("launch", "processes matching the hint outlived SIGKILL");
    }
    return sent;
}

} // namespace opensu::launch

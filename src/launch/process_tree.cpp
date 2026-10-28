#include "process_tree.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <thread>

#include "lucent/log.h"
#include "process_table.hpp"

namespace opensu::launch {
namespace {

/// Rounds of find-and-kill before giving up on a tree that keeps forking.
constexpr int killRounds = 10;
constexpr auto settle = std::chrono::milliseconds{20};

} // namespace

bool ProcessTree::available() {
    return table::available();
}

std::vector<ProcessId> ProcessTree::matching(const std::string& hint) {
    std::vector<ProcessId> found;
    if (hint.empty()) {
        return found;
    }
    for (const ProcessId pid : table::pids()) {
        if (table::commandLine(pid).find(hint) != std::string::npos) {
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
    for (const ProcessId pid : table::pids()) {
        if (const ProcessId parent = table::parentOf(pid); parent >= 0) {
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
            if (table::kill(pid)) {
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

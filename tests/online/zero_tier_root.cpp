// A ZeroTier root for tools.nat_lab: `online_zero_tier_root <storage> <address/port> <roots file>`
// makes a root identity in `storage`, writes the signed root set naming it at that endpoint for
// the shells, and runs the root node until it is killed.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include "lucent/p2p/zerotier.h"
#include "online/zero_tier.hpp"
#include "support/test_main.hpp"

int main(int argc, char** argv) try {
    if (argc != 4) {
        std::fputs("usage: online_zero_tier_root <storage> <address/port> <roots file>\n", stderr);
        return 2;
    }
    const std::filesystem::path storage = argv[1];
    const std::string endpoint = argv[2];
    std::string error;
    const std::optional<std::string> roots = lucent::p2p::make_root(storage, endpoint, error);
    if (!roots) {
        std::fprintf(stderr, "no root: %s\n", error.c_str());
        return 1;
    }
    std::ofstream{argv[3], std::ios::binary} << *roots;
    const auto port =
        static_cast<std::uint16_t>(std::stoi(endpoint.substr(endpoint.rfind('/') + 1)));
    const auto node =
        lucent::p2p::ZeroTierNode::start({.storage = storage,
                                          .roots = *roots,
                                          .port = port,
                                          .network_port = opensu::online::zeroTierPort},
                                         error);
    if (!node) {
        std::fprintf(stderr, "no root node: %s\n", error.c_str());
        return 1;
    }
    std::puts("root running");
    std::fflush(stdout);
    while (node) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

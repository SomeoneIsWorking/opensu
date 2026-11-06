// What a rollback session starts from, through the shipping `StateSync` over an in-memory link: one
// state offered and nothing crossing, a differing one crossing despite loss, and a peer's
// oversized, undersized or tampered state refused before it could reach a core.
#include "netplay/state_sync.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "check.hpp"
#include "fileio/little_endian.hpp"
#include "support/test_main.hpp"

namespace {

using namespace std::chrono_literals;
using opensu::netplay::StateSync;
using opensu::netplay::Transport;
using opensu::test::expect;
using Outcome = StateSync::Outcome;

constexpr char offerKind = 16;
constexpr char chunkKind = 18;

class Link final : public Transport {
  public:
    Link* other{nullptr};
    std::deque<std::string> arrived;
    int dropEvery{0};
    int sent{0};
    std::size_t chunksSent{0};
    std::function<void(std::string&)> tamper;

    void send(std::string_view message) override {
        ++sent;
        if (message.front() == chunkKind) {
            ++chunksSent;
        }
        if (dropEvery != 0 && sent % dropEvery == 0) {
            return;
        }
        std::string passed{message};
        if (tamper) {
            tamper(passed);
        }
        other->arrived.push_back(std::move(passed));
    }

    std::optional<std::string> receive(std::chrono::milliseconds /*wait*/) override {
        if (arrived.empty()) {
            return std::nullopt;
        }
        std::string message = std::move(arrived.front());
        arrived.pop_front();
        return message;
    }
};

struct Pair {
    Link offers;
    Link takes;

    Pair() {
        offers.other = &takes;
        takes.other = &offers;
    }
};

std::vector<std::byte> stateOf(std::size_t size, unsigned seed) {
    std::vector<std::byte> state(size);
    for (std::size_t at = 0; at < size; ++at) {
        state[at] = static_cast<std::byte>((at * 31 + seed) & 0xFFU);
    }
    return state;
}

Outcome settle(StateSync& offers, StateSync& takes) {
    Outcome outcome = Outcome::Running;
    for (int round = 0; round < 40000 && outcome == Outcome::Running; ++round) {
        offers.pump(0ms);
        outcome = takes.pump(0ms);
        std::this_thread::sleep_for(100us);
    }
    return outcome;
}

void oneStateCrossesNothing() {
    Pair pair;
    const auto state = stateOf(50'000, 1);
    StateSync offers{pair.offers, StateSync::Role::Offers, state};
    StateSync takes{pair.takes, StateSync::Role::Takes, state};
    expect(settle(offers, takes) == Outcome::Same, "the machines agree on the hash");
    expect(pair.offers.chunksSent == 0, "and no byte of a state crosses");
    expect(takes.state().empty(), "so there is nothing to load");
    offers.pump(0ms);
    expect(offers.pump(0ms) == Outcome::Same, "the offering side hears it");
}

void aDifferingStateCrossesDespiteLoss() {
    Pair pair;
    pair.offers.dropEvery = 7;
    const auto theirs = stateOf(50'500, 1);
    const auto own = stateOf(50'500, 2);
    StateSync offers{pair.offers, StateSync::Role::Offers, theirs};
    StateSync takes{pair.takes, StateSync::Role::Takes, own};
    expect(settle(offers, takes) == Outcome::Received, "the state crosses");
    expect(std::ranges::equal(takes.state(), theirs), "whole");
}

void anOversizedStateIsRefusedUnallocated() {
    Pair pair;
    const auto own = stateOf(1000, 2);
    StateSync takes{pair.takes, StateSync::Role::Takes, own};
    std::string offer(1, offerKind);
    opensu::fileio::appendLittleEndian(offer, std::uint64_t{1} << 40U, 8);
    offer.append(32, 'x');
    pair.takes.arrived.push_back(offer);
    expect(takes.pump(0ms) == Outcome::Refused, "a terabyte is not this core's size");
    expect(takes.state().empty(), "nothing was allocated for it");
}

void anUndersizedStateIsRefused() {
    Pair pair;
    const auto theirs = stateOf(999, 1);
    const auto own = stateOf(1000, 2);
    StateSync offers{pair.offers, StateSync::Role::Offers, theirs};
    StateSync takes{pair.takes, StateSync::Role::Takes, own};
    expect(settle(offers, takes) == Outcome::Refused, "any size but the core's is refused");
}

void aTamperedStateNeverReachesTheCaller() {
    Pair pair;
    pair.offers.tamper = [](std::string& message) {
        if (message.front() == chunkKind && message.size() > 100) {
            message[100] = static_cast<char>(message[100] ^ 1);
        }
    };
    const auto theirs = stateOf(5000, 1);
    const auto own = stateOf(5000, 2);
    StateSync offers{pair.offers, StateSync::Role::Offers, theirs};
    StateSync takes{pair.takes, StateSync::Role::Takes, own};
    expect(settle(offers, takes) == Outcome::Refused, "what does not hash is refused");
    expect(takes.state().empty(), "and is not kept");
}

void anOversizedChunkIsIgnored() {
    Pair pair;
    const auto theirs = stateOf(1500, 1);
    const auto own = stateOf(1500, 2);
    StateSync offers{pair.offers, StateSync::Role::Offers, theirs};
    StateSync takes{pair.takes, StateSync::Role::Takes, own};
    offers.pump(0ms);
    takes.pump(0ms);
    std::string chunk(1, chunkKind);
    opensu::fileio::appendLittleEndian(chunk, std::uint32_t{0}, 4);
    chunk.append(4000, 'x');
    pair.takes.arrived.push_back(chunk);
    expect(settle(offers, takes) == Outcome::Received, "a chunk past its size changes nothing");
    expect(std::ranges::equal(takes.state(), theirs), "the state is still the offered one");
}

} // namespace

int main() try {
    oneStateCrossesNothing();
    aDifferingStateCrossesDespiteLoss();
    anOversizedStateIsRefusedUnallocated();
    anUndersizedStateIsRefused();
    aTamperedStateNeverReachesTheCaller();
    anOversizedChunkIsIgnored();
    std::printf("state_sync: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

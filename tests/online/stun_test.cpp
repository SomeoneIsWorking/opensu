// A STUN answer: the mapped address read, and anything not an answer to our request refused.
#include "online/stun.hpp"

#include <cstdio>
#include <string>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::online;
using opensu::test::expect;

/// A Binding success for `id` naming 192.0.2.1:32853, as RFC 5769's sample masks it.
std::string answerTo(const StunId& id) {
    std::string message{"\x01\x01\x00\x0C\x21\x12\xA4\x42", 8};
    message.append(reinterpret_cast<const char*>(id.data()), id.size());
    message.append("\x00\x20\x00\x08\x00\x01\xA1\x47\xE1\x12\xA6\x43", 12);
    return message;
}

void readsTheAddress() {
    const StunId id = newStunId();
    const auto mapped = stunMapped(answerTo(id), id);
    expect(mapped && mapped->port == 32853, "the port is unmasked");
    expect(mapped == opensu::stream::udpAddress("192.0.2.1", 32853), "the address is unmasked");
}

void refusesTheRest() {
    const StunId id = newStunId();
    expect(!stunMapped(answerTo(newStunId()), id), "another transaction's answer");
    expect(!stunMapped(stunRequest(id), id), "a request is no answer");
    std::string cut = answerTo(id);
    cut.resize(cut.size() - 4);
    expect(!stunMapped(cut, id), "an attribute cut short");
    std::string huge = answerTo(id);
    huge[23] = '\x7F';
    expect(!stunMapped(huge, id), "an attribute longer than the message");
    expect(!stunMapped("", id) && !isStun("\x03\x0D"), "too short to be STUN");
    expect(isStun(stunRequest(id)), "our own request is STUN");
}

} // namespace

int main() try {
    readsTheAddress();
    refusesTheRest();
    std::printf("stun: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

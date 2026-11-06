// Invite and reply codes: what goes in comes out through whatever a chat app does to them, and a
// damaged, foreign or wrong-kind code is named as such.
#include "online/invite_code.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "check.hpp"
#include "support/test_main.hpp"

namespace {

using namespace opensu::online;
using opensu::test::expect;

Reach reach() {
    Reach reach;
    reach.port = 40123;
    reach.publics = {{.ip = 0x0100007FU, .port = 4000}, {.ip = 0x0200007FU, .port = 5000}};
    reach.locals = {0x0101A8C0U};
    reach.varying = true;
    return reach;
}

Invite invite() {
    return {.secret = opensu::secrets::crypto::randomBytes(secretBytes), .reach = reach()};
}

void roundTrip() {
    const Invite made = invite();
    const std::string code = writeInvite(made);
    CodeProblem problem{};
    const std::optional<Invite> read = readInvite(code, problem);
    expect(read && read->secret == made.secret && read->reach == made.reach,
           "an invite reads back");
    expect(code.size() < 80, "an invite is short enough to paste");
    const std::string reply = writeReply(reach(), made.secret);
    const std::optional<Reach> replied = readReply(reply, made.secret, problem);
    expect(replied && *replied == reach(), "a reply reads back");
}

void chatApps() {
    const Invite made = invite();
    const std::string code = writeInvite(made);
    std::string lower = spaced(code);
    std::ranges::transform(lower, lower.begin(), [](unsigned char letter) {
        return static_cast<char>(std::tolower(letter));
    });
    CodeProblem problem{};
    expect(readInvite("  " + lower + "\r\n", problem).has_value(), "spaces and case do not matter");
}

void refusals() {
    const Invite made = invite();
    const std::string code = writeInvite(made);
    CodeProblem problem{};
    expect(!readInvite("", problem) && problem == CodeProblem::Empty, "nothing pasted");
    expect(!readInvite("hello there", problem) && problem == CodeProblem::Unknown, "not a code");
    std::string damaged = code;
    damaged[20] = damaged[20] == 'A' ? 'B' : 'A';
    expect(!readInvite(damaged, problem) && problem == CodeProblem::Damaged, "a changed letter");
    expect(!readInvite(code.substr(0, code.size() - 3), problem) && problem == CodeProblem::Damaged,
           "a cut code");
    const std::string reply = writeReply(reach(), made.secret);
    expect(!readInvite(reply, problem) && problem == CodeProblem::WrongKind,
           "a reply as an invite");
    expect(!readReply(code, made.secret, problem) && problem == CodeProblem::WrongKind,
           "an invite as a reply");
    const Invite other = invite();
    expect(!readReply(reply, other.secret, problem) && problem == CodeProblem::OtherInvite,
           "a reply to another invite");
}

void tooMany() {
    Invite made = invite();
    made.reach.locals.assign(mostAddresses + 2, 0x0101A8C0U);
    CodeProblem problem{};
    const std::optional<Invite> read = readInvite(writeInvite(made), problem);
    expect(read && read->reach.locals.size() == mostAddresses, "a code holds what fits");
}

} // namespace

int main() try {
    roundTrip();
    chatApps();
    refusals();
    tooMany();
    std::printf("invite codes: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}

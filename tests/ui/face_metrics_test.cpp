// The shipped face's vertical metrics and characters, read the way Typeface reads them.
#include "face_metrics.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::test::fail;
using opensu::test::near;
using opensu::ui::readFaceMetrics;

void calSans(const char* path) {
    std::ifstream stream{path, std::ios::binary};
    expect(static_cast<bool>(stream), "the shipped face opens");
    const std::vector<char> bytes{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
    const auto metrics = readFaceMetrics(std::as_bytes(std::span{bytes}));
    if (!metrics) {
        fail("head and hhea are found");
    }
    // Cal Sans 1.000, as iiSU ships it: 1000 units per em, hhea 1000 / -300.
    near(metrics->unitsPerEm, 1000.0f, "units per em");
    near(metrics->ascent, 1000.0f, "ascent");
    near(metrics->descent, -300.0f, "descent");
    near(metrics->lineBoxPerEm(), 1.3f, "stb's line box is 1.3 em");
    // Store titles carry ™ and ®; the atlas once stopped at a hand list and drew them as "?".
    const auto has = [&](int c) {
        return std::ranges::binary_search(metrics->codepoints, c);
    };
    expect(has('A') && has(0xE9) && has(0x2122) && has(0xAE) && has(0x203A),
           "the cmap gives ASCII, Latin-1, trade mark, registered and the single guillemet");
    expect(metrics->codepoints.size() == 547, "Cal Sans maps 547 characters");
    expect(std::ranges::is_sorted(metrics->codepoints), "characters are ascending");
}

void truncated() {
    const std::array<std::byte, 8> stub{};
    expect(!readFaceMetrics(stub).has_value(), "a truncated file has no metrics");
}

} // namespace

int main(int argc, char** argv) {
    expect(argc == 2, "usage: face_metrics_test FONT");
    calSans(argv[1]);
    truncated();
    std::printf("face_metrics: all checks passed\n");
    return 0;
}

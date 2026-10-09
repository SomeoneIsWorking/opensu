// The platform table, built from the compiled frame gradients.
#include "platform.hpp"

#include <cstdio>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::Platforms;

} // namespace

int main() {
    const Platforms platforms;
    expect(platforms.size() == opensu::ui::strokeGradientCount(), "one platform per gradient");
    const auto* psx = platforms.find("PSX");
    expect(psx != nullptr, "a key matches whatever its case");
    expect(psx->strokeFrom == 0xb8b0de && psx->strokeTo == 0xc7acff, "with its gradient");
    const auto* steam = platforms.forSource(opensu::library::Source::Steam);
    expect(steam != nullptr && steam->key == "steam", "a Steam title is framed as Steam");
    expect(platforms.forSource(opensu::library::Source::Rom) == nullptr,
           "a ROM is framed by its system, not its source");
    expect(platforms.find("no-such-console") == nullptr, "an unknown key has no frame");
    std::printf("platform: all checks passed\n");
    return 0;
}

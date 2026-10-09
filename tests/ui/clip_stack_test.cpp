// Nested clips: an inner clip stays inside the outer one and ending it restores the outer one,
// which raylib's scissor alone does not do.
#include "clip_stack.hpp"

#include <cstdio>
#include <optional>
#include <vector>

#include "check.hpp"

namespace {

using opensu::test::expect;
using opensu::ui::ClipStack;
using opensu::ui::Rect;
using opensu::ui::ScopedClip;

bool same(const std::optional<Rect>& a, const Rect& b) {
    return a && a->x == b.x && a->y == b.y && a->width == b.width && a->height == b.height;
}

void nested() {
    std::vector<std::optional<Rect>> applied;
    {
        ClipStack clips{[&](std::optional<Rect> area) {
            applied.push_back(area);
        }};
        const Rect panel{100, 100, 400, 300};
        const ScopedClip content{clips, panel};
        {
            // A card scrolled half above the panel.
            const ScopedClip card{clips, Rect{150, 50, 100, 100}};
            expect(same(applied.back(), Rect{150, 100, 100, 50}),
                   "the card clips inside the panel");
        }
        expect(same(applied.back(), panel), "ending the card's clip restores the panel's");
    }
    expect(applied.size() == 4 && !applied.back().has_value(), "the last pop stops clipping");
}

void disjoint() {
    std::optional<Rect> last;
    ClipStack clips{[&](std::optional<Rect> area) {
        last = area;
    }};
    clips.push(Rect{0, 0, 10, 10});
    clips.push(Rect{20, 20, 10, 10});
    expect(last && last->width == 0 && last->height == 0, "areas that miss clip everything");
    clips.pop();
    clips.pop();
    clips.pop();
    expect(!last.has_value(), "an extra pop is harmless");
}

} // namespace

int main() {
    nested();
    disjoint();
    std::printf("clip_stack: all checks passed\n");
    return 0;
}

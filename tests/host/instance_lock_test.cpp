// instance_lock: one openSU at a time, so a second launch cannot start a second Steam.
#include "instance_lock.hpp"

#include <cstdio>
#include <filesystem>

#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

void oneAtATime() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "instance_lock"};
    const std::filesystem::path file = scratch.path() / "run" / "instance.lock";
    host::InstanceLock first{file};
    expect(first.held(), "the first openSU takes it");
    {
        host::InstanceLock second{file};
        expect(!second.held(), "a second cannot, even in the same process");
    }
    host::InstanceLock third{file};
    expect(!third.held(), "and it stays held while the first lives");
}

void releasedWhenTheFirstLeaves() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "instance_lock_release"};
    const std::filesystem::path file = scratch.path() / "instance.lock";
    {
        host::InstanceLock first{file};
        expect(first.held(), "taken");
    }
    host::InstanceLock again{file};
    expect(again.held(), "dropped, the next openSU takes it");
}

} // namespace

int main() {
    oneAtATime();
    releasedWhenTheFirstLeaves();
    std::printf("instance_lock: all checks passed\n");
    return 0;
}

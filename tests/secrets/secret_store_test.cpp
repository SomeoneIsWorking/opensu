// The secret store's file place: owner-only file and folder, read back, replaced, forgotten. The
// keyring is never asked here, so a test run leaves the player's keyring alone.
#include "secrets/secret_store.hpp"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
using opensu::secrets::SecretPlace;
using opensu::secrets::SecretStore;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

mode_t modeOf(const fs::path& path) {
    struct stat info{};
    expect(::stat(path.c_str(), &info) == 0, "the path exists");
    return info.st_mode & 0777U;
}

void testFileKeepsASecretOwnerOnly() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "secret-store"};
    const fs::path dir = scratch.path() / "config" / "secrets";
    const SecretStore store{dir, false};
    std::string error;
    expect(!store.read("key", error) && error.empty(), "nothing is kept at first");

    const std::optional<SecretPlace> place = store.save("key", "a key", "first", error);
    expect(place == SecretPlace::File, "without the keyring the secret goes to a file");
    expect(modeOf(dir / "key") == 0600, "the file is the owner's alone");
    expect(modeOf(dir) == 0700, "and so is its folder");
    const std::optional<SecretStore::Found> found = store.read("key", error);
    expect(found && found->value == "first" && found->place == SecretPlace::File,
           "the secret reads back from the file");

    expect(store.save("key", "a key", "second", error).has_value(), "a secret is replaced");
    const std::optional<SecretStore::Found> replaced = store.read("key", error);
    expect(replaced && replaced->value == "second", "by the new one");
    expect(modeOf(dir / "key") == 0600, "still owner-only");

    expect(store.forget("key", error), "forgetting succeeds");
    expect(!fs::exists(dir / "key"), "forgetting removes the file");
    expect(!store.read("key", error), "a forgotten secret is gone");
    expect(store.forget("key", error), "forgetting nothing is fine");
}

} // namespace

int main() {
    testFileKeepsASecretOwnerOnly();
    std::printf("secret_store: all checks passed\n");
    return 0;
}

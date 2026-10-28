"""Command line: `uv run --frozen python -m tools.package.android build`.

Builds openSU for arm64-v8a and x86_64 against the shared android-port prefix (built once when
missing), stages the Gradle project, assembles and signs the release APK with the key in
OPENSU_ANDROID_KEYSTORE (base64) or OPENSU_ANDROID_KEYSTORE_FILE, with
OPENSU_ANDROID_KEYSTORE_PASSWORD, OPENSU_ANDROID_KEY_ALIAS and OPENSU_ANDROID_KEY_PASSWORD, checks
it and writes build/android/out/openSU-<version>-android.apk and its .sha256.
"""

import argparse
import hashlib
import shutil

from tools.check import deps, process
from tools.package.android import apk, config, native, port, project, signing, version


def build() -> int:
    layout = config.layout_from_environment()
    env = process.build_environment(layout.scratch)
    sources = deps.Deps(layout.sources)
    deps.fetch(sources, env)
    android_port = port.load(layout.android_port, env)
    profiles = {abi: native.ensure_prefix(layout, android_port, abi) for abi in config.ABIS}
    for abi in config.ABIS:
        native.build(layout, sources, abi, env)
    gradle_project = project.stage(layout, android_port)
    for abi, profile in profiles.items():
        project.stage_libraries(layout, android_port, profile, abi)
    name = version.describe()
    java = android_port.select_java_home(minimum=17, maximum=26)
    # Gradle refuses two SDK variables that spell the same SDK differently.
    gradle_env = {
        **{name: value for name, value in env.items() if name != "ANDROID_SDK_ROOT"},
        **signing.gradle_environment(env, layout.scratch),
        "ANDROID_HOME": str(layout.sdk),
        "JAVA_HOME": str(java),
        "OPENSU_ANDROID_VERSION_NAME": name,
        "OPENSU_ANDROID_VERSION_CODE": str(version.code(name)),
    }
    process.run(
        ["./gradlew", "--no-daemon", "--console=plain", ":app:assembleRelease"],
        gradle_env,
        cwd=gradle_project,
    )
    built = gradle_project / "app/build/outputs/apk/release/app-release.apk"
    signer = apk.inspect(built, layout, android_port)
    layout.out.mkdir(parents=True, exist_ok=True)
    final = layout.out / f"openSU-{name}-android.apk"
    shutil.copy2(built, final)
    digest = hashlib.sha256(final.read_bytes()).hexdigest()
    (layout.out / f"{final.name}.sha256").write_text(f"{digest}  {final.name}\n")
    print(f"{final}\nsigner SHA-256 {signer}", flush=True)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="tools.package.android", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )  # fmt: skip
    parser.add_argument("command", choices=("build",))
    parser.parse_args(argv)
    return build()

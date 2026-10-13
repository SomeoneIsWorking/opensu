"""The AppImage build inside the build container: compile, test, install, bundle, pack."""

import shutil
from pathlib import Path

from tools.appimage import appdir, bundle, config, pack
from tools.check import cmake, deps, process

# Everything but the `session` component: a session entry naming the AppImage's mount would break
# once the mount is gone.
INSTALL_COMPONENT = "Unspecified"
SIGNIN_XPI = "opensu-signin.xpi"


def plan_for(layout: config.Layout) -> cmake.Plan:
    return cmake.Plan(
        build_dir=layout.root,
        deps=deps.Deps(layout.deps),
        gamescope="native",
        build_type="Release",
        install_prefix="/usr",
        defines=(
            f"-DOPENSU_GAMESCOPE_MESON_OPTIONS={config.GAMESCOPE_MESON_OPTIONS}",
            "-DOPENSU_DESKTOP_EXEC=opensu",
        ),
    )


def build(layout: config.Layout, version: str) -> Path:
    env = process.build_environment(layout.scratch)
    plan = plan_for(layout)
    deps.fetch(plan.deps, env)
    cmake.configure(plan, env)
    # The tests need a systemd user manager; CI runs them in tools.check's container.
    cmake.build(plan, env)
    if layout.appdir.exists():
        shutil.rmtree(layout.appdir)
    cmake.install(plan, layout.appdir, INSTALL_COMPONENT, env)
    bundled = bundle.bundle(layout.appdir / "usr", config.GLIBC_FLOOR, env)
    print(f"bundled {len(bundled)} libraries: {' '.join(bundled)}", flush=True)
    appdir.finish(layout.appdir)
    appimage = pack.pack(layout, version, env)
    # The browser loads the sign-in extension, so the release carries it beside the AppImage.
    shutil.copy2(layout.appdir / "usr" / "share" / "opensu" / SIGNIN_XPI, layout.out / SIGNIN_XPI)
    return appimage

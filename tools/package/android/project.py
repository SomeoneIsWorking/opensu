"""The Gradle project staged under build/android/project: the sources in packaging/android, SDL's
and android-port's Java, the assets and every ABI's libraries."""

import shutil
from pathlib import Path
from types import ModuleType

from tools.package.android import config


def stage(layout: config.Layout, port: ModuleType) -> Path:
    project = layout.project
    if project.exists():
        shutil.rmtree(project)
    shutil.copytree(config.SOURCE / "app", project / "app")
    for name in ("build.gradle", "settings.gradle"):
        shutil.copy2(config.SOURCE / name, project / name)
    wrapper = project / "gradle" / "wrapper"
    wrapper.mkdir(parents=True)
    shutil.copy2(config.SOURCE / "gradle-wrapper.properties", wrapper)
    port.stage_gradle_runtime(layout.prefix(config.ABIS[0]), project)
    # The Activity copies these to filesDir/share/opensu, where the shell reads them as files.
    shutil.copytree(config.REPO / "assets", project / "app/src/main/assets/opensu")
    return project


def stage_libraries(layout: config.Layout, port: ModuleType, profile: object, abi: str) -> None:
    """libmain.so, SDL3 and libc++ through android-port; then the runner and librashader beside
    them, where `installLayout` looks."""
    port.stage_package_runtime(profile)
    built = layout.native_dir(abi)
    for library in (
        built / "src" / "libopensu-retro.so",
        built / "lib" / "opensu" / "librashader.so",
    ):
        if not library.is_file():
            raise config.refuse(f"the {abi} build has no {library.name}")
        shutil.copy2(library, profile.jni_libs / abi / library.name)

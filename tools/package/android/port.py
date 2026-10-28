"""The pinned android-port checkout's tool module: prefix builds, staging and APK checks."""

import importlib.util
import sys
from collections.abc import Mapping
from pathlib import Path
from types import ModuleType

from tools.check import deps
from tools.check.pins import ANDROID_PORT


def load(checkout: Path, env: Mapping[str, str]) -> ModuleType:
    """android-port at its pin, imported from `checkout`."""
    deps.checkout(ANDROID_PORT, checkout, env)
    tool = checkout / "tools" / "android_port.py"
    spec = importlib.util.spec_from_file_location("opensu_android_port", tool)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot import {tool}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module

"""An Android Virtual Device that runs openSU's APK for the lab: booted, driven over adb and
stopped by the run that booted it."""

import json
import shlex
import subprocess
import time
from pathlib import Path

from tools.package.android import config

# The console port the device is booted on; adb names it emulator-<port>.
CONSOLE_PORT = 5584
BOOT_SECONDS = 300


class AndroidDevice:
    """`avd` booted with `memory_mb` of RAM, its app `config.APPLICATION_ID` driven as that app."""

    def __init__(self, avd: str, memory_mb: int, log: Path) -> None:
        sdk = config.layout_from_environment().sdk
        self.adb_words = [str(sdk / "platform-tools" / "adb"), "-s", f"emulator-{CONSOLE_PORT}"]
        self.package = config.APPLICATION_ID
        self.files = f"/data/user/0/{self.package}/files"
        self.forwards: list[int] = []
        log.parent.mkdir(parents=True, exist_ok=True)
        self.process = subprocess.Popen(
            [
                str(sdk / "emulator" / "emulator"),
                "-avd",
                avd,
                "-no-window",
                "-no-audio",
                "-no-snapshot",
                "-no-boot-anim",
                "-memory",
                str(memory_mb),
                "-gpu",
                "host",
                "-port",
                str(CONSOLE_PORT),
            ],
            stdout=log.open("ab"),
            stderr=subprocess.STDOUT,
        )
        print(f"emulator pid {self.process.pid}", flush=True)
        try:
            self.wait_for_boot(log)
        except BaseException:
            self.stop()
            raise

    def wait_for_boot(self, log: Path) -> None:
        subprocess.run([*self.adb_words, "wait-for-device"], check=True, timeout=BOOT_SECONDS)
        until = time.monotonic() + BOOT_SECONDS
        while self.adb("shell", "getprop", "sys.boot_completed", check=False).strip() != "1":
            if self.process.poll() is not None or time.monotonic() > until:
                raise RuntimeError(f"the AVD did not boot: {log}")
            time.sleep(2)
        print("the AVD booted", flush=True)

    def adb(self, *words: str, check: bool = True) -> str:
        done = subprocess.run(
            [*self.adb_words, *words], check=check, capture_output=True, text=True
        )
        return done.stdout

    def as_app(self, script: str, check: bool = True) -> str:
        """`script` run as the app in its data folder; adb joins its words into one line."""
        return self.adb("shell", f"run-as {self.package} sh -c {shlex.quote(script)}", check=check)

    def push(self, local: Path, inside: str) -> None:
        """`local` to `inside`, a path under the app's data folder, where run-as starts."""
        staged = f"/data/local/tmp/{local.name}"
        self.adb("push", str(local), staged)
        self.as_app(f"mkdir -p {Path(inside).parent} && cp {staged} {inside}")

    def install(self, apk: Path) -> None:
        self.adb("install", "-r", str(apk))

    def forward(self, local: int, remote: int) -> None:
        self.adb("forward", f"tcp:{local}", f"tcp:{remote}")
        self.forwards.append(local)

    def start_app(self) -> None:
        self.adb("shell", "am", "start", "-n", f"{self.package}/.ShellActivity")

    def stop_app(self) -> None:
        self.adb("shell", "am", "force-stop", self.package, check=False)

    def app_running(self) -> bool:
        return bool(self.adb("shell", "pidof", self.package, check=False).strip())

    def runner_port(self, seconds: float = 30.0) -> int:
        """The game runner's control port, from the control file it hands the shell."""
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            found = self.as_app(
                "for f in $(find . -name 'retro-*.json' 2>/dev/null); do cat $f; echo; done",
                check=False,
            )
            for line in found.splitlines():
                if '"port"' in line:
                    return int(json.loads(line)["port"])
            time.sleep(0.5)
        raise RuntimeError("the device's runner never announced itself")

    def logcat(self) -> str:
        return self.adb("logcat", "-d", check=False)

    def stop(self) -> None:
        """The app, the forwards and the emulator this run booted, by its process."""
        if self.process.poll() is None:
            self.stop_app()
            for local in self.forwards:
                self.adb("forward", "--remove", f"tcp:{local}", check=False)
            self.process.terminate()
            try:
                self.process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

"""Stops a run on this machine before it starves the rest of it of memory."""

import os
import signal
import threading
from pathlib import Path
from types import FrameType, TracebackType


class LowMemory(RuntimeError):
    pass


def available_kib() -> int:
    for line in Path("/proc/meminfo").read_text().splitlines():
        if line.startswith("MemAvailable:"):
            return int(line.split()[1])
    raise RuntimeError("/proc/meminfo has no MemAvailable")


class MemoryFloor:
    """Within it, the main thread gets `LowMemory` once available memory falls under `floor_kib`,
    so the run's own cleanup stops what it started."""

    def __init__(self, floor_kib: int) -> None:
        self.floor_kib = floor_kib
        self.done = threading.Event()
        self.watcher = threading.Thread(target=self.watch, daemon=True)

    def watch(self) -> None:
        while not self.done.wait(1.0):
            if available_kib() < self.floor_kib:
                print(f"available memory {available_kib() // 1024} MiB: stopping", flush=True)
                os.kill(os.getpid(), signal.SIGUSR1)
                return

    @staticmethod
    def interrupt(_signal: int, _frame: FrameType | None) -> None:
        raise LowMemory("available memory fell under the floor")

    def __enter__(self) -> "MemoryFloor":
        signal.signal(signal.SIGUSR1, self.interrupt)
        self.watcher.start()
        return self

    def __exit__(
        self,
        kind: type[BaseException] | None,
        error: BaseException | None,
        trace: TracebackType | None,
    ) -> None:
        self.done.set()
        signal.signal(signal.SIGUSR1, signal.SIG_DFL)

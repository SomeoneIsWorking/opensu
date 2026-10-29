"""Programs in the test user's desktop session, where a player's would run.

The guest agent's commands run in session 0, which has no desktop. A scheduled task with an
interactive principal runs in the logged-on user's session instead; each job is a batch file that
records its output and exit code under the harness folder for `wait` to read.
"""

import time

from tools.vm.windows.agent import Agent, AgentError
from tools.vm.windows.config import GUEST_DIR, TEST_USER


def _task_script(name: str, batch: str) -> str:
    return f"""
$action = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c "{batch}"'
$principal = New-ScheduledTaskPrincipal -UserId '{TEST_USER}' -LogonType Interactive
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName 'opensu-{name}' -Action $action -Principal $principal `
    -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName 'opensu-{name}'
"""


def start(agent: Agent, name: str, lines: list[str]) -> None:
    """Runs `lines` as a batch file in the user's session and returns at once."""
    job = f"{GUEST_DIR}\\{name}-job.cmd"
    batch = f"{GUEST_DIR}\\{name}.cmd"
    done = f"{GUEST_DIR}\\{name}.done"
    # The job is called, so an `exit /b` in it still reaches the exit code; the code is written
    # whole, then renamed, so `wait` never reads a half-written one.
    wrapper = [
        "@echo off",
        f'call "{job}"',
        f'>"{done}.part" echo %ERRORLEVEL%',
        f'move /y "{done}.part" "{done}" >nul',
    ]
    agent.run("cmd.exe", ["/c", "mkdir", GUEST_DIR])
    agent.run("cmd.exe", ["/c", "del", done])
    agent.write_file(job, ("\r\n".join(["@echo off", *lines]) + "\r\n").encode())
    agent.write_file(batch, ("\r\n".join(wrapper) + "\r\n").encode())
    started = agent.powershell(_task_script(name, batch))
    if started.code != 0:
        raise AgentError(f"could not start task {name}: {started.out}{started.err}")


def wait(agent: Agent, name: str, timeout_s: float = 600) -> int:
    """The exit code of the job `name`, once its batch file ends."""
    done = f"{GUEST_DIR}\\{name}.done"
    deadline = time.monotonic() + timeout_s
    while not agent.exists(done):
        if time.monotonic() > deadline:
            raise AgentError(f"task {name} still ran after {timeout_s}s")
        time.sleep(1)
    return int(agent.read_file(done).decode(errors="replace").strip() or "-1")


def run(agent: Agent, name: str, lines: list[str], timeout_s: float = 600) -> int:
    start(agent, name, lines)
    return wait(agent, name, timeout_s)

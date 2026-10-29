"""Command line of the Windows guest: `uv run --frozen python -m tools.vm.windows <command>`."""

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

from tools.vm import machine as vm_machine
from tools.vm.qmp import Qmp
from tools.vm.windows import agent, desktop, machine, opensu, provision
from tools.vm.windows import config as winconfig
from tools.vm.windows.agent import Agent


def _provision(args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    provision.install(cfg)
    provision.wait(cfg, args.timeout)
    provision.finish(cfg)
    return 0


def _up(_args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    disk = vm_machine.new_overlay(cfg, cfg.base_image)
    machine.fresh_run_state(cfg, cfg.base_vars, cfg.base_tpm)
    pid = machine.launch(cfg, disk)
    agent.wait_ready(cfg.agent_socket)
    print(f"qemu pid {pid}, guest agent on {cfg.agent_socket}")
    return 0


def _down(_args: argparse.Namespace) -> int:
    machine.stop(winconfig.load())
    return 0


def _status(_args: argparse.Namespace) -> int:
    pid = vm_machine.running_pid(winconfig.load())
    print(f"running pid {pid}" if pid else "not running")
    return 0


def _shot(args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    stamp = datetime.now().strftime("%H%M%S")
    with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
        print(qmp.screendump(cfg.shots / f"{args.name}-{stamp}.png"))
    return 0


def _key(args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
        for chord in args.chords:
            qmp.send_key(chord.split("+"))
            time.sleep(0.2)
    return 0


def _system(args: argparse.Namespace) -> int:
    with Agent(winconfig.load().agent_socket) as guest:
        done = guest.run("cmd.exe", ["/c", " ".join(args.command)], args.timeout)
    sys.stdout.write(done.out + done.err)
    return done.code


def _desktop(args: argparse.Namespace) -> int:
    with Agent(winconfig.load().agent_socket) as guest:
        code = desktop.run(guest, "job", [" ".join(args.command)], args.timeout)
    return code


def _put(args: argparse.Namespace) -> int:
    with Agent(winconfig.load().agent_socket) as guest:
        guest.write_file(args.remote, Path(args.local).read_bytes())
    return 0


def _get(args: argparse.Namespace) -> int:
    with Agent(winconfig.load().agent_socket) as guest:
        Path(args.local).write_bytes(guest.read_file(args.remote))
    return 0


def _deploy(args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    with Agent(cfg.agent_socket) as guest:
        opensu.install_lavapipe(cfg, guest)
        opensu.install(guest, Path(args.installer))
        print(f"test ROM in {opensu.place_test_rom(cfg, guest)}")
    return 0


def _opensu(args: argparse.Namespace) -> int:
    with Agent(winconfig.load().agent_socket) as guest:
        if args.action == "start":
            opensu.start(guest, " ".join(args.arguments))
        else:
            opensu.stop(guest)
    return 0


def _ui(args: argparse.Namespace) -> int:
    cfg = winconfig.load()
    with Agent(cfg.agent_socket) as guest:
        if args.action == "state":
            print(opensu.state(guest))
        elif args.action == "press":
            for button in args.args:
                opensu.press(guest, button)
                time.sleep(0.4)
        else:
            stamp = datetime.now().strftime("%H%M%S")
            print(opensu.frame(guest, cfg.shots / f"{args.args[0]}-{stamp}.png"))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.vm.windows")
    sub = parser.add_subparsers(dest="command", required=True)

    def add(name: str, func, help_text: str) -> argparse.ArgumentParser:
        command = sub.add_parser(name, help=help_text)
        command.set_defaults(func=func)
        return command

    add("provision", _provision, "unattended install, saved as base.qcow2").add_argument(
        "--timeout", type=int, default=3 * 3600
    )
    add("up", _up, "boot a fresh overlay of the base and wait for the guest agent")
    add("down", _down, "power the VM off")
    add("status", _status, "is the VM running")
    add("shot", _shot, "QMP screendump of the desktop to PNG").add_argument("name")
    add("key", _key, "send chords such as ctrl+alt+delete ret").add_argument("chords", nargs="+")
    system = add("system", _system, "a cmd.exe command as LocalSystem (session 0)")
    system.add_argument("--timeout", type=int, default=600)
    system.add_argument("command", nargs="+")
    user = add("desktop", _desktop, "a cmd.exe command in the test user's desktop session")
    user.add_argument("--timeout", type=int, default=600)
    user.add_argument("command", nargs="+")
    put = add("put", _put, "copy a host file into the guest")
    put.add_argument("local")
    put.add_argument("remote")
    get = add("get", _get, "copy a guest file to the host")
    get.add_argument("remote")
    get.add_argument("local")
    deploy = add("deploy", _deploy, "lavapipe, the installer run silently, the test ROM")
    deploy.add_argument("installer", help="the CI artifact's openSU-*-setup.exe")
    run = add("opensu", _opensu, "start openSU on the desktop, or stop it by its PID")
    run.add_argument("action", choices=["start", "stop"])
    run.add_argument("arguments", nargs="*")
    ui = add("ui", _ui, "openSU's control channel: state | press BUTTON... | frame NAME")
    ui.add_argument("action", choices=["state", "press", "frame"])
    ui.add_argument("args", nargs="*")
    args = parser.parse_args(argv)
    return args.func(args)

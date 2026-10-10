"""Command line of the VM harness: `uv run --frozen python -m tools.vm <command>`."""

import argparse
import sys
import time
from datetime import datetime

from tools.vm import config as vmconfig
from tools.vm import configure, control, deploy, machine, provision, vnc
from tools.vm.guest import Guest
from tools.vm.qmp import Qmp


def _provision(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    provision.install(cfg)
    machine.wait_exit(cfg, args.timeout)
    provision.finish(cfg)
    return 0


def _up(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    backing = cfg.snapshot(args.snapshot) if args.snapshot else cfg.base_image
    disk = machine.new_overlay(cfg, backing)
    pid = machine.launch(cfg, disk, args.gpu)
    guest = Guest(cfg)
    guest.wait_ready()
    if args.autologin is not None:
        session = None if args.autologin == "off" else args.autologin
        configure.set_autologin(guest, session)
        configure.reboot(guest)
    print(f"qemu pid {pid}, ssh port {cfg.ssh_port}, gpu {args.gpu}")
    return 0


def _down(_args: argparse.Namespace) -> int:
    machine.stop(vmconfig.load())
    return 0


def _status(_args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    pid = machine.running_pid(cfg)
    print(f"running pid {pid}" if pid else "not running")
    return 0


def _ssh(args: argparse.Namespace) -> int:
    result = Guest(vmconfig.load()).run(" ".join(args.command), sudo=args.sudo, check=False)
    sys.stdout.write(result.out)
    return result.code


def _qmp(cfg: vmconfig.VmConfig) -> Qmp:
    return Qmp(cfg.qmp_socket, cfg.width, cfg.height)


def _shot(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    stamp = datetime.now().strftime("%H%M%S")
    path = cfg.shots / f"{args.name}-{stamp}.png"
    if cfg.vnc_socket.exists():
        print(vnc.capture(cfg.vnc_socket, path))
        return 0
    with _qmp(cfg) as qmp:
        print(qmp.screendump(path))
    return 0


def _key(args: argparse.Namespace) -> int:
    with _qmp(vmconfig.load()) as qmp:
        for chord in args.chords:
            qmp.send_key(chord.split("+"))
            time.sleep(0.2)
    return 0


def _type(args: argparse.Namespace) -> int:
    with _qmp(vmconfig.load()) as qmp:
        qmp.type_text(args.text)
    return 0


def _click(args: argparse.Namespace) -> int:
    with _qmp(vmconfig.load()) as qmp:
        qmp.click(args.x, args.y, args.button)
    return 0


def _deploy(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    deploy.deploy(cfg, Guest(cfg), args.fork, args.headless_gamescope)
    return 0


def _session_entry(_args: argparse.Namespace) -> int:
    sys.stdout.write(deploy.install_session_entry(Guest(vmconfig.load())))
    return 0


def _ui(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    guest = Guest(cfg)
    if args.action == "state":
        print(control.state(guest))
    elif args.action == "press":
        for button in args.args:
            control.press(guest, button)
            time.sleep(0.4)
    elif args.action == "key":
        control.key(guest, args.args[0])
    elif args.action == "text":
        control.text(guest, args.args[0])
    else:
        stamp = datetime.now().strftime("%H%M%S")
        print(control.frame(guest, cfg.shots / f"{args.args[0]}-{stamp}.png"))
    return 0


def _autologin(args: argparse.Namespace) -> int:
    guest = Guest(vmconfig.load())
    if args.session == "default":
        configure.clear_autologin_override(guest)
    else:
        configure.set_autologin(guest, None if args.session == "off" else args.session)
    configure.reboot(guest)
    return 0


def _snapshot(args: argparse.Namespace) -> int:
    cfg = vmconfig.load()
    if machine.running_pid(cfg) is not None:
        raise RuntimeError("stop the VM before saving a snapshot")
    machine.flatten(cfg.overlay, cfg.snapshot(args.name))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.vm")
    sub = parser.add_subparsers(dest="command", required=True)

    def add(name: str, func, help_text: str) -> argparse.ArgumentParser:
        command = sub.add_parser(name, help=help_text)
        command.set_defaults(func=func)
        return command

    add(
        "provision", _provision, "install and configure the guest, then save base.qcow2"
    ).add_argument("--timeout", type=int, default=5400)
    up = add("up", _up, "boot a fresh overlay of base.qcow2 (or a snapshot)")
    up.add_argument("--snapshot", help="boot an overlay of snap-NAME instead of the base")
    up.add_argument("--gpu", choices=machine.GPU_MODES, default="virtio")
    up.add_argument("--autologin", help="off, or a session name; omit for the host-like default")
    add("down", _down, "power the VM off")
    add("status", _status, "is the VM running")
    ssh = add("ssh", _ssh, "run a command in the guest")
    ssh.add_argument("--sudo", action="store_true")
    ssh.add_argument("command", nargs="+")
    add("shot", _shot, "QMP screendump to PNG").add_argument("name")
    add("key", _key, "send chords such as ctrl+alt+f2 ret").add_argument("chords", nargs="+")
    add("type", _type, "type text").add_argument("text")
    click = add("click", _click, "click at guest pixel X Y")
    click.add_argument("x", type=int)
    click.add_argument("y", type=int)
    click.add_argument("--button", default="left")
    dep = add("deploy", _deploy, "build the current tree and install it into the guest ~/.local")
    dep.add_argument("--fork", type=lambda p: __import__("pathlib").Path(p), default=None)
    dep.add_argument(
        "--headless-gamescope", action="store_true", help="run the DRM backend headless"
    )
    ui = add(
        "ui",
        _ui,
        "openSU control channel: state | press BUTTON... | key CHORD | text TEXT | frame NAME",
    )
    ui.add_argument("action", choices=["state", "press", "key", "text", "frame"])
    ui.add_argument("args", nargs="*")
    add("session-entry", _session_entry, "the README's sudo install of the session entry")
    auto = add("autologin", _autologin, "set SDDM autologin: off, default or a session name")
    auto.add_argument("session")
    add("snapshot", _snapshot, "flatten the stopped VM's overlay into snap-NAME").add_argument(
        "name"
    )
    args = parser.parse_args(argv)
    return args.func(args)

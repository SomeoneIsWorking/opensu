import json
import socket
import threading
from pathlib import Path
from xml.etree import ElementTree

import pytest

from tools.vm import fetch, hostfiles, kickstart, machine, test_rom
from tools.vm.config import VmConfig
from tools.vm.qmp import Qmp, keys_for_char
from tools.vm.windows import machine as winmachine
from tools.vm.windows import unattend
from tools.vm.windows.config import WinConfig


def test_keys_for_char():
    assert keys_for_char("a") == ["a"]
    assert keys_for_char("A") == ["shift", "a"]
    assert keys_for_char("!") == ["shift", "1"]
    assert keys_for_char("-") == ["minus"]
    with pytest.raises(ValueError):
        keys_for_char("é")


def test_treeinfo_checksums():
    text = (
        "[checksums]\nimages/pxeboot/vmlinuz = sha256:ab\nimages/x = md5:cd\n"
        "[general]\nk = sha256:ff\n"
    )
    assert fetch.parse_treeinfo_checksums(text) == {"images/pxeboot/vmlinuz": "ab"}


def test_kickstart_has_user_key_and_packages():
    text = kickstart.render("ssh-ed25519 AAA key", "44", 32)
    assert "ssh-ed25519 AAA key" in text
    assert "--groups=wheel" in text
    assert "NOPASSWD" not in text
    assert "\ngamescope\n" in text and "\nsddm\n" in text
    assert "repo=fedora-44" in text


def test_sddm_conf_retargets_user():
    conf = "[Autologin]\nRelogin=true\nSession=steam-picker\nUser=somebody\n\n[Users]\n"
    out = hostfiles.retarget_autologin_user(conf, "tester")
    assert "User=tester" in out and "somebody" not in out and "Session=steam-picker" in out


def test_command_line(tmp_path: Path):
    cfg = VmConfig(root=tmp_path, shots=tmp_path / "shots")
    plain = machine.build_command(cfg, tmp_path / "d.qcow2", "virtio", [])
    assert "-display" in plain and "none" in plain and "virtio-gpu-pci" in plain
    assert f"hostfwd=tcp:127.0.0.1:{cfg.ssh_port}-:22" in " ".join(plain)
    venus = " ".join(machine.build_command(cfg, tmp_path / "d.qcow2", "venus", []))
    assert "venus=true" in venus and "egl-headless" in venus


def test_qmp_screendump_and_click(tmp_path: Path):
    path = tmp_path / "qmp.sock"
    server = socket.socket(socket.AF_UNIX)
    server.bind(str(path))
    server.listen(1)
    seen: list[dict] = []

    def serve():
        conn, _ = server.accept()
        stream = conn.makefile("rw")
        stream.write(json.dumps({"QMP": {}}) + "\n")
        stream.flush()
        for line in stream:
            seen.append(json.loads(line))
            stream.write(json.dumps({"return": {}}) + "\n")
            stream.flush()

    threading.Thread(target=serve, daemon=True).start()
    with Qmp(path, 1281, 801) as qmp:
        qmp.screendump(tmp_path / "out" / "a.png")
        qmp.move(1280, 800)
    server.close()
    names = [m["execute"] for m in seen]
    assert names == ["qmp_capabilities", "screendump", "input-send-event"]
    assert seen[1]["arguments"]["format"] == "png"
    events = seen[2]["arguments"]["events"]
    assert [e["data"]["value"] for e in events] == [0x7FFF, 0x7FFF]


def test_qmp_close_releases_the_socket(tmp_path: Path):
    path = tmp_path / "qmp.sock"
    server = socket.socket(socket.AF_UNIX)
    server.bind(str(path))
    server.listen(1)
    closed = threading.Event()

    def serve():
        conn, _ = server.accept()
        stream = conn.makefile("rw")
        stream.write(json.dumps({"QMP": {}}) + "\n")
        stream.flush()
        stream.readline()
        stream.write(json.dumps({"return": {}}) + "\n")
        stream.flush()
        if conn.recv(1) == b"":
            closed.set()

    threading.Thread(target=serve, daemon=True).start()
    with Qmp(path, 640, 480):
        pass
    assert closed.wait(5)
    server.close()


def test_test_rom_is_ines_with_vectors():
    rom = test_rom.build()
    assert rom[:4] == b"NES\x1a" and rom[4] == 1 and rom[5] == 1
    prg = rom[16 : 16 + test_rom.PRG_SIZE]
    reset = int.from_bytes(prg[-4:-2], "little")
    assert reset == test_rom.PRG_BASE and prg[0] == 0x78
    assert len(rom) == 16 + test_rom.PRG_SIZE + test_rom.CHR_SIZE


def test_windows_answer_file():
    text = unattend.render_answer_file()
    root = ElementTree.fromstring(text)
    assert root.tag.endswith("unattend")
    assert "D:\\viostor\\w11\\amd64" in text and "H:\\NetKVM\\w11\\amd64" in text
    assert "$" not in text
    batch = unattend.render_first_logon()
    assert "qemu-ga-x86_64.msi" in batch and batch.endswith("\r\n")


def test_windows_command_line(tmp_path: Path):
    cfg = WinConfig(root=tmp_path, shots=tmp_path / "shots")
    line = " ".join(winmachine.build_command(cfg, tmp_path / "d.qcow2", [tmp_path / "a.iso"]))
    assert "smm=on" in line and "tpm-tis" in line and "org.qemu.guest_agent.0" in line
    assert "ide-cd,drive=cd0,bus=ide.0,bootindex=1" in line
    assert "virtio-blk-pci,drive=disk0,bootindex=2" in line

import json
import socket
import threading
from pathlib import Path

import pytest

from tools.vm import fetch, hostfiles, kickstart, machine
from tools.vm.config import VmConfig
from tools.vm.qmp import Qmp, keys_for_char


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

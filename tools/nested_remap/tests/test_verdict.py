from tools.nested_remap.verdict import judge

KWIN_ERROR = (
    "[11:01:52.464282] {Display Queue} wl_display#1.error(xdg_surface#50, 3, "
    '"attached a buffer before configure event")\n'
    "[gamescope] [Error] xdg_backend: Failed to dispatch input thread queue: "
    "protocol error 3 on xdg_surface@50\n"
)


def test_host_error_message_wins():
    verdict = judge(134, KWIN_ERROR)
    assert not verdict.survived
    expected = "xdg_surface#50 error 3: attached a buffer before configure event"
    assert verdict.protocol_error == expected


def test_gamescope_line_without_wayland_debug():
    verdict = judge(134, KWIN_ERROR.splitlines()[1])
    assert verdict.protocol_error == "xdg_surface#50 error 3"


def test_clean_exit_survives():
    assert judge(0, "[gamescope] [Info] reaper: done\n").survived


def test_nonzero_exit_without_protocol_error_dies():
    verdict = judge(1, "")
    assert not verdict.survived
    assert verdict.protocol_error is None

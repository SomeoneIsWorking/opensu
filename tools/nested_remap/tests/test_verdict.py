from tools.nested_remap.verdict import judge

KWIN_ERROR = (
    "[11:01:52.464282] {Display Queue} wl_display#1.error(xdg_surface#50, 3, "
    '"attached a buffer before configure event")\n'
    "[gamescope] [Error] xdg_backend: Failed to dispatch input thread queue: "
    "protocol error 3 on xdg_surface@50\n"
)


def test_host_error_message_wins():
    verdict = judge(134, KWIN_ERROR)
    assert not verdict.passed
    expected = "xdg_surface#50 error 3: attached a buffer before configure event"
    assert verdict.protocol_error == expected


def test_gamescope_line_without_wayland_debug():
    verdict = judge(134, KWIN_ERROR.splitlines()[1])
    assert verdict.protocol_error == "xdg_surface#50 error 3"


def test_clean_exit_survives():
    assert judge(0, "[gamescope] [Info] reaper: done\n").passed


def test_nonzero_exit_without_protocol_error_dies():
    verdict = judge(1, "")
    assert not verdict.passed
    assert verdict.protocol_error is None


def test_host_loss_passes_on_a_failure_status():
    assert judge(1, "", host_lost=True).passed


def test_host_loss_fails_on_a_signal_or_a_clean_quit():
    assert not judge(-6, "", host_lost=True).passed
    assert not judge(0, "", host_lost=True).passed

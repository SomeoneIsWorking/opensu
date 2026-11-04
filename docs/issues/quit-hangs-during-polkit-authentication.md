# Quitting openSU while a polkit authentication waits hangs the quit

Status: fixed.

Observed: a hidden openSU, Install session mode, the card waiting on the authentication (a
stand-in helper's 20 s wait, or the real helper's password prompt); `POST /quit` or SIGTERM:
openSU does not exit while the authentication waits, with `pkexec /bin/true` and the helper still
its children. With the real helper it never exits; with the stand-in, only once the helper's own
wait ends 20 s later. Expected: the quit cancels the authentication and openSU exits.

Reproduced 2026-11-04 on a323100 with `OPENSU_POLKIT_HELPER` (`phone-wait`: `PAM_TEXT_INFO`, then
`sleep 20`): after `POST /quit`, and separately after SIGTERM, still running 5 s later. gdb on the
hung pid, both ways: the main thread in `ShellApp::~ShellApp` → `~SessionInstallController` →
`std::future` waiting for the install's `std::async`; that thread in `SddmSessionMode::install`
running pkexec; the agent's thread in `SdBusPolkitAgent::run` polling with the authentication
pending.

Cause: `src/app/session_install_controller.hpp` `SessionInstallController::~SessionInstallController`
waited (through its `std::async` future) for pkexec, which waits for polkit, which waits for the
reply from openSU's agent in `HostServices::polkit`, and the agent replies only when the player
answers or cancels, or when it is itself destroyed, after the controller.

Fix: the install flow names its dependency on the agent (`SessionInstallController` takes the
`host::PolkitAgent`), and its destructor calls `PolkitAgent::shutDown` before it waits.
`shutDown` cancels the authentication under way through the path Cancel takes (polkit gets
`Cancelled`, the helper's process group is ended, pkexec exits 126) and cancels every
BeginAuthentication after it, so a pkexec started just before the quit cannot wait either.

Shown 2026-11-04 in hidden runs (`scratch/polkit-pam/quit/` in the main checkout), with
`OPENSU_STEAMGRIDDB_API` pointed at a closed port so no cover fit is in flight: during the
stand-in's wait, `POST /quit` exits in 0.06 s and SIGTERM in 0.03 s; during the real helper's
password prompt `POST /quit` exits in 0.04 s; each time "openSU is closing; the authentication is
cancelled", the install said cancelled, and pkexec and the helper gone. A cover fit in flight still
delays any quit by up to a few seconds (`docs/issues/quit-waits-for-a-cover-fit.md`).

Left: without openSU's own agent (no system bus or no helper), pkexec waits on the desktop's agent
and a quit still waits for that dialog to be answered; killing pkexec there could cut the root
installer off mid-way, so it is not done.

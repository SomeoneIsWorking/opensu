# Quitting openSU while a polkit authentication waits hangs the quit

Status: open.

Observed 2026-11-04: a hidden openSU with the real helper, Install session mode, a wrong password,
the card asking again; `POST /quit`, then SIGTERM: openSU never exits, with `pkexec /bin/true` and
the setuid `polkit-agent-helper-1` still its children. SIGKILL ends it, and polkit then ends both.
Expected: the quit cancels the authentication and openSU exits.

Suspected cause, from reading, not yet traced: `src/app/shell_app.hpp` declares `sessionInstall_`
after `host_`, so `SessionInstallController::job_` (an `std::async` future) is destroyed first and
waits for pkexec, which waits for polkit, which waits for the agent in `host_.polkit`, whose
authentication waits for an answer nobody gives.

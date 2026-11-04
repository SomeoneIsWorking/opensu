# The polkit card shows nothing while a non-password PAM module works

Status: fixed.

Observed: while a PAM module that asks nothing works (fprintd, howdy, pam_u2f, a remote-biometry
module that sends `PAM_TEXT_INFO` "Confirm on <phone> (fingerprint), or wait for the password" and
waits up to 20 s), openSU shows no card, so the player sees nothing and cannot cancel; the module's
text is never shown until a password prompt comes, and a module that succeeds without any prompt
shows nothing at all. Expected: the card opens when polkit begins the authentication and shows
what PAM says as it says it.

Reproduced 2026-11-04 on 8f96437 (HEAD plus the `OPENSU_POLKIT_HELPER` seam): hidden openSU,
`OPENSU_ROOT_STEP=/bin/true`, a stand-in helper writing `PAM_TEXT_INFO`, 8 s, `PAM_ERROR_MSG`, 3 s,
`PAM_PROMPT_ECHO_OFF`; Guide, Power, Install session mode, Install. Two seconds in, only the
install's progress card shows (no polkit card, no text); the card first appears with the prompt
11 s in.

Cause: `src/host/polkit_authentication.cpp` `Authentication::prompt` returned a prompt only while
a question was asked (`asking_`, set on `PAM_PROMPT_ECHO_OFF`/`ON` alone), and
`src/app/polkit_prompt_controller.cpp` `PolkitPromptController::service` opened a card only for a
new question serial, so an authentication that was waiting on a module had no state to show.

Fix: `Authentication` owns the card's state: `Prompt::State` `Waiting` from `begin` and after each
answer, `Asking` for each question (its own `serial`), the latest `PAM_TEXT_INFO`/`PAM_ERROR_MSG`
as the note, and a number per authentication (`Prompt::authentication`); `prompt()` is present for
the whole of a pending authentication. The agent shows a submitted answer or a cancel at once
(`withInbox`). The controller renders that state on one `ui::AuthenticationScreen` per
authentication, read on every build: waiting with a sweeping bar and Cancel, a field added for each
question, the note changed in place, closed on the verdict. Cancel while waiting replies
`Cancelled` to polkit and drops the helper, whose process group is ended.

Shown 2026-11-04 in hidden runs (`scratch/polkit-pam/` in the main checkout, `frames/`, with the
stand-ins in `helpers/`): a note then `SUCCESS` with no prompt (`1a` card at once, `1b` note
replaced, `1c` closed); a note, a module's failure, then a field (`2a`, `2b`, `2c`, `2d` masked);
a hidden question, waiting while checked, `Failed to match`, then a shown one (`3a` to `3e`,
`3d` the code as typed); Cancel during a 20 s wait (`4a`, `4b`; the helper's shell and its
`sleep` gone from its process group). Real helper, `pkexec /bin/true`: field, waiting through
`pam_faildelay`, a wrong password asked again, Cancel (`5a` to `5d`, `5f`).

Driving a real PAM service against openSU's agent: polkit's helper always authenticates through
the `polkit-1` PAM service, so a test PAM service is reached through `OPENSU_POLKIT_HELPER`: a
program that takes the user as its argument and polkit's cookie on stdin, runs `pam_start` on the
test service for that user with a conversation that writes each message as one line
(`PAM_PROMPT_ECHO_OFF <text>`, `PAM_PROMPT_ECHO_ON <text>`, `PAM_ERROR_MSG <text>`,
`PAM_TEXT_INFO <text>`, the text `g_strescape`d) and reads each answer as one line from stdin,
then writes `SUCCESS` or `FAILURE`. Run openSU with `--hidden`, `DISABLE_LSFG=1`,
`OPENSU_ROOT_STEP=/bin/true` and `OPENSU_POLKIT_HELPER=<that program>`, then Guide, Power,
Install session mode, Install over the control channel. polkit itself then refuses `/bin/true`
(the stand-in cannot answer polkit), which says nothing about the card.

#!/bin/sh
# install-session.sh — the one root step that makes openSU a login session you can switch to.
# openSU builds this script in, puts it with root-install.sh and the two files it installs in a
# directory of its own (<runtime dir>/session-install) and runs it there through polkit:
#
#   pkexec /bin/sh <runtime dir>/session-install/install-session.sh
#
# From that directory it installs
#   opensu.desktop          to /usr/local/share/wayland-sessions/opensu.desktop, the entry SDDM lists
#   opensu-session-select   to /usr/local/libexec/opensu-session-select, the root-owned selector
# and writes /etc/sudoers.d/zz-opensu-session-select, which lets the user pkexec ran it for run the
# selector. The rule names the selector with each of its two fixed arguments and nothing else, so
# it grants "start openSU / go back" and no other root command.

here=$(cd "$(dirname "$0")" && pwd)
. "$here/root-install.sh"

selector=/usr/local/libexec/opensu-session-select
entry=/usr/local/share/wayland-sessions/opensu.desktop
rule=/etc/sudoers.d/zz-opensu-session-select

place 0755 "$here/opensu-session-select" "$selector"
place 0644 "$here/opensu.desktop" "$entry"
sudoers_rule "$rule" "$user may switch the login session between openSU and the desktop." \
    "$user ALL=(root) NOPASSWD: $selector opensu, $selector restore"
relabel "$selector" "$entry" "$rule"

echo "opensu-install: done"

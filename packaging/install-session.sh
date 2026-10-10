#!/bin/sh
# install-session.sh — the one root step that makes openSU a login session you can switch to.
# Run through a single sudo, from the install tree (<prefix>/libexec/opensu/):
#
#   sudo sh <prefix>/libexec/opensu/install-session.sh
#
# openSU's "Install session mode" runs exactly that, with the password on sudo's stdin. It installs
#   /usr/local/share/wayland-sessions/opensu.desktop       the entry SDDM lists (from <prefix>)
#   /usr/local/libexec/opensu-session-select               the root-owned session selector
#   /etc/sudoers.d/zz-opensu-session-select                lets the calling user run the selector
# The sudoers rule names the selector with each of its two fixed arguments and nothing else, so it
# grants "start openSU / go back" and no other root command. It goes in last, after visudo -cf
# accepts it, so a failure part-way never leaves a rule without its selector.
# OPENSU_SELECT_ROOT prefixes the targets, but only for a non-root caller (tests): sudo drops it.

set -eu
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin
LC_ALL=C
export PATH LC_ALL

# The first line, which tells openSU that sudo accepted the password and this script began.
echo "opensu-install: start"

fail() {
    echo "opensu-install: $*" >&2
    exit 1
}

root=
if [ "$(id -u)" -eq 0 ]; then
    :
elif [ -n "${OPENSU_SELECT_ROOT:-}" ]; then
    root=$OPENSU_SELECT_ROOT
else
    fail "must run as root, through sudo"
fi

user=${SUDO_USER:-}
case $user in
'' | root | [!A-Za-z_]* | *[!A-Za-z0-9_.-]*) fail "run it through sudo as a normal user" ;;
esac
id -u -- "$user" >/dev/null 2>&1 || fail "no such user: $user"

here=$(cd "$(dirname "$0")" && pwd)
prefix=$(cd "$here/../.." && pwd)
entry_source=$prefix/share/wayland-sessions/opensu.desktop
selector_source=$here/opensu-session-select
[ -f "$entry_source" ] || fail "$entry_source is missing; run cmake --install first"
[ -f "$selector_source" ] || fail "$selector_source is missing; run cmake --install first"

selector=/usr/local/libexec/opensu-session-select
entry=/usr/local/share/wayland-sessions/opensu.desktop
rule=/etc/sudoers.d/zz-opensu-session-select

# Files belong to root; a test run as a normal user cannot chown.
place() {
    if [ -z "$root" ]; then
        install -D -m "$1" -o root -g root "$2" "$3"
    else
        install -D -m "$1" "$2" "$3"
    fi
}
place 0755 "$selector_source" "$root$selector"
place 0644 "$entry_source" "$root$entry"

mkdir -p "$root/etc/sudoers.d"
staged=$root/etc/sudoers.d/.zz-opensu-session-select.new
trap 'rm -f -- "$staged"' EXIT
{
    echo "# Installed by openSU: $user may switch the login session between openSU and the desktop."
    echo "$user ALL=(root) NOPASSWD: $selector opensu, $selector restore"
} >"$staged"
chmod 0440 "$staged"
visudo -cf "$staged" >/dev/null || fail "visudo rejected the sudoers rule"
[ -n "$root" ] || chown root:root "$staged"
mv -f -- "$staged" "$root$rule"
if [ -z "$root" ] && command -v restorecon >/dev/null 2>&1; then
    restorecon -F "$selector" "$entry" "$rule"
fi

echo "opensu-install: done"

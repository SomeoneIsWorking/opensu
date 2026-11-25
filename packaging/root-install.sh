# root-install.sh — what every openSU installer run through pkexec shares, sourced from the stage
# directory the installer runs in (`host/root_install`). It prints the marker that tells openSU
# polkit let the installer run, then sets `root` (the target prefix, empty on a real machine) and
# `user` (who pkexec ran it for, fit for a sudoers rule).
# OPENSU_SELECT_ROOT prefixes the targets, but only for a non-root caller (tests): pkexec drops it.

set -eu
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin
LC_ALL=C
export PATH LC_ALL

# The first line, which tells openSU that polkit let it run and the installer began.
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
    fail "must run as root, through pkexec"
fi

# pkexec names the user who asked by uid.
uid=${PKEXEC_UID:-}
case $uid in
'' | 0 | *[!0-9]*) fail "run it through pkexec as a normal user" ;;
esac
user=$(id -nu -- "$uid" 2>/dev/null) || fail "no user has uid $uid"
case $user in
'' | root | [!A-Za-z_]* | *[!A-Za-z0-9_.-]*) fail "the user name $user cannot go in a sudoers rule" ;;
esac

# place MODE SOURCE TARGET: installs SOURCE at TARGET under the prefix, root's on a real machine.
place() {
    [ -f "$2" ] || fail "$2 is missing"
    if [ -z "$root" ]; then
        install -D -m "$1" -o root -g root "$2" "$root$3"
    else
        install -D -m "$1" "$2" "$root$3"
    fi
}

# sudoers_rule RULE COMMENT LINE: writes the rule, mode 0440, once visudo -cf accepts it. It goes in
# last, so a failure part-way never leaves a rule without what it names.
sudoers_rule() {
    mkdir -p "$root/etc/sudoers.d"
    staged=$root/etc/sudoers.d/.$(basename "$1").new
    trap 'rm -f -- "$staged"' EXIT
    {
        echo "# Installed by openSU: $2"
        echo "$3"
    } >"$staged"
    chmod 0440 "$staged"
    visudo -cf "$staged" >/dev/null || fail "visudo rejected the sudoers rule"
    [ -n "$root" ] || chown root:root "$staged"
    mv -f -- "$staged" "$root$1"
}

# relabel TARGET...: gives SELinux's labels to what was installed, where SELinux is.
relabel() {
    if [ -z "$root" ] && command -v restorecon >/dev/null 2>&1; then
        restorecon -F "$@"
    fi
}

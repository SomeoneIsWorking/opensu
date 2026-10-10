#!/bin/sh
# opensu-session-select opensu|restore
#
# Installed root-owned as /usr/local/libexec/opensu-session-select and run by the user through one
# sudoers rule (see install-session.sh). Chooses which session SDDM starts at the next login.
# POSIX sh on purpose: it runs as root on a machine openSU does not control, so it needs no
# interpreter beyond /bin/sh, no module path and no environment.
#
#   opensu   the next login is openSU. With SDDM autologin this writes the drop-in
#            /etc/sddm.conf.d/zz-opensu-session.conf (it sorts after kde_settings.conf, so its
#            Session= wins); without autologin it sets SDDM's remembered last session instead
#            (/var/lib/sddm/state.conf, [Last] Session=).
#   restore  undoes that: removes the drop-in and puts the remembered last session back.
#
# What was there before is kept in /var/lib/opensu/previous-session the first time.
# OPENSU_SELECT_ROOT prefixes every path, but only for a non-root caller (tests): sudo drops it.

set -eu
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin
LC_ALL=C
export PATH LC_ALL

fail() {
    echo "opensu-session-select: $*" >&2
    exit 1
}

[ "$#" -eq 1 ] || fail "usage: opensu-session-select opensu|restore"
case $1 in
opensu | restore) mode=$1 ;;
*) fail "the argument must be opensu or restore" ;;
esac

root=
if [ "$(id -u)" -eq 0 ]; then
    :
elif [ -n "${OPENSU_SELECT_ROOT:-}" ]; then
    root=$OPENSU_SELECT_ROOT
else
    fail "must run as root, through sudo"
fi

# sudo sets SUDO_USER; it is only compared with the autologin user and never written anywhere.
user=${SUDO_USER:-}
case $user in
'' | root | [!A-Za-z_]* | *[!A-Za-z0-9_.-]*) fail "run it through sudo as a normal user" ;;
esac

entry=/usr/local/share/wayland-sessions/opensu.desktop
dropin=$root/etc/sddm.conf.d/zz-opensu-session.conf
main_conf=$root/etc/sddm.conf
state_conf=$root/var/lib/sddm/state.conf
memory=$root/var/lib/opensu/previous-session

# SDDM's config files in the order it reads them, the last assignment winning; without our own.
config_stream() {
    for dir in "$root/usr/lib/sddm/sddm.conf.d" "$root/etc/sddm.conf.d"; do
        for file in "$dir"/*.conf; do
            if [ -f "$file" ] && [ "$file" != "$dropin" ]; then
                cat -- "$file"
                printf '\n[]\n'
            fi
        done
    done
    if [ -f "$main_conf" ]; then
        cat -- "$main_conf"
    fi
}

# The value of $2 in section $1 on stdin; the last one wins, empty when there is none.
ini_value() {
    awk -v want_section="$1" -v want_key="$2" '
        { sub(/\r$/, "") }
        /^[ \t]*[#;]/ { next }
        /^[ \t]*\[/ { s = $0; gsub(/^[ \t]*\[|\][ \t]*$/, "", s); section = s; next }
        section == want_section {
            i = index($0, "=")
            if (i == 0) { next }
            k = substr($0, 1, i - 1); gsub(/^[ \t]+|[ \t]+$/, "", k)
            if (k != want_key) { next }
            v = substr($0, i + 1); gsub(/^[ \t]+|[ \t]+$/, "", v)
            value = v
        }
        END { print value }'
}

# Sets [Last] Session= in SDDM's state file to $1, or removes the key when $1 is empty.
set_last_session() {
    mkdir -p "$(dirname "$state_conf")"
    [ -f "$state_conf" ] || : >"$state_conf"
    tmp=$(mktemp "$state_conf.XXXXXX")
    awk -v value="$1" '
        function emit() { if (!done && value != "") { print "Session=" value }; done = 1 }
        /^[ \t]*\[/ {
            if (in_last) { emit() }
            s = $0; gsub(/^[ \t]*\[|\][ \t]*$/, "", s)
            in_last = (s == "Last"); seen = seen || in_last
            print; next
        }
        in_last && /^[ \t]*Session[ \t]*=/ { emit(); next }
        { print }
        END {
            if (in_last) { emit() }
            if (!seen && value != "") { print "[Last]"; print "Session=" value }
        }' "$state_conf" >"$tmp"
    chmod 644 "$tmp"
    mv -f "$tmp" "$state_conf"
}

last_session() {
    if [ -f "$state_conf" ]; then
        ini_value Last Session <"$state_conf"
    fi
}

autologin_user=$(config_stream | ini_value Autologin User)

remember() {
    [ -f "$memory" ] && return 0
    mkdir -p "$(dirname "$memory")"
    {
        echo "autologin=$(config_stream | ini_value Autologin Session)"
        echo "last=$(last_session)"
    } >"$memory.new"
    mv -f "$memory.new" "$memory"
}

remembered_last() {
    [ -f "$memory" ] || return 0
    sed -n 's/^last=//p' "$memory"
}

case $mode in
opensu)
    [ -f "$root$entry" ] || fail "the openSU session entry is not installed ($entry)"
    remember
    if [ -n "$autologin_user" ]; then
        [ "$autologin_user" = "$user" ] || fail "SDDM autologin belongs to another user"
        # /etc/sddm.conf is read after the drop-ins, so a Session= there would beat ours.
        if [ -f "$main_conf" ] && [ -n "$(ini_value Autologin Session <"$main_conf")" ]; then
            fail "/etc/sddm.conf sets the autologin session, which overrides the drop-in"
        fi
        mkdir -p "$(dirname "$dropin")"
        printf '%s\n' '# Written by opensu-session-select; "restore" removes it.' \
            '[Autologin]' 'Session=opensu' >"$dropin.new"
        mv -f "$dropin.new" "$dropin"
        echo "opensu-session-select: autologin session is now opensu"
    else
        set_last_session "$entry"
        echo "opensu-session-select: last session is now $entry"
    fi
    ;;
restore)
    rm -f -- "$dropin"
    if [ -f "$memory" ]; then
        if [ "$(last_session)" = "$entry" ]; then
            set_last_session "$(remembered_last)"
        fi
        rm -f -- "$memory"
    fi
    echo "opensu-session-select: the previous session is back"
    ;;
esac

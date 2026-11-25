#!/bin/sh
# opensu-ring-lights RRGGBB
#
# Installed root-owned as /usr/local/libexec/opensu-ring-lights and run by the user through one
# sudoers rule (see install-ring-lights.sh). Sets the ROG Ally's (and Ally X's) stick rings to one
# solid colour with the controller's own RGB command; 000000 turns them off. The kernel's LED class
# cannot do it: it caps each zone's packed 0xRRGGBB at 255, so only blue gets through.
# POSIX sh on purpose: it runs as root on a machine openSU does not control.
#
# The controller's configuration interface is the hidraw node of ASUS's 0b05:1abe or 0b05:1b4c
# whose report descriptor has the vendor page 0xff31 and report 0x5a. Each command is one 64-byte
# report, 0x5a first, zero padded:
#   5a "ASUS Tech.Inc."        start talking to the light controller
#   5a ba c5 c4 LEVEL          the hardware brightness, 0 (off) to 3
#   5a b3 00 00 RR GG BB       every zone, static, this colour
#   5a b5, then 5a b4          set, then apply
# OPENSU_SELECT_ROOT prefixes /sys and /dev, but only for a non-root caller (tests): sudo drops it.

set -eu
umask 022
PATH=/usr/sbin:/usr/bin:/sbin:/bin
LC_ALL=C
export PATH LC_ALL

fail() {
    echo "opensu-ring-lights: $*" >&2
    exit 1
}

[ "$#" -eq 1 ] || fail "usage: opensu-ring-lights RRGGBB"
case $1 in
[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) colour=$1 ;;
*) fail "the colour must be six lower-case hex digits" ;;
esac

root=
if [ "$(id -u)" -eq 0 ]; then
    :
elif [ -n "${OPENSU_SELECT_ROOT:-}" ]; then
    root=$OPENSU_SELECT_ROOT
else
    fail "must run as root, through sudo"
fi

node=
for dir in "$root"/sys/class/hidraw/hidraw*; do
    [ -f "$dir/device/uevent" ] || continue
    case $(grep '^HID_ID=' "$dir/device/uevent") in
    HID_ID=0003:00000B05:00001ABE | HID_ID=0003:00000B05:00001B4C) ;;
    *) continue ;;
    esac
    descriptor=" $(od -An -tx1 -v "$dir/device/report_descriptor" | tr -s ' \n' '  ') "
    case $descriptor in
    *" 06 31 ff "*" 85 5a "*) node=$root/dev/${dir##*/} && break ;;
    esac
done
[ -n "$node" ] || fail "no ROG Ally controller is here"

# report BYTE...: one zero-padded 64-byte report, written whole.
report() {
    bytes=
    count=0
    for byte in "$@"; do
        bytes="$bytes\\0$(printf '%o' "0x$byte")"
        count=$((count + 1))
    done
    while [ "$count" -lt 64 ]; do
        bytes="$bytes\\0"
        count=$((count + 1))
    done
    printf '%b' "$bytes" | dd of="$node" bs=64 count=1 iflag=fullblock oflag=append conv=notrunc \
        status=none
}

red=${colour%????}
green=${colour#??}
green=${green%??}
blue=${colour#????}
level=03
[ "$colour" != 000000 ] || level=00

report 5a 41 53 55 53 20 54 65 63 68 2e 49 6e 63 2e
report 5a ba c5 c4 "$level"
report 5a b3 00 00 "$red" "$green" "$blue"
report 5a b5
report 5a b4

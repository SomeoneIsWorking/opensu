#!/bin/sh
# install-ring-lights.sh — the one root step that lets openSU set the stick lights' colour where the
# kernel cannot. openSU builds this script in, puts it with root-install.sh and the helper in a
# directory of its own (<runtime dir>/ring-lights-install) and runs it there through polkit:
#
#   pkexec /bin/sh <runtime dir>/ring-lights-install/install-ring-lights.sh
#
# It installs opensu-ring-lights to /usr/local/libexec/opensu-ring-lights, root-owned, and writes
# /etc/sudoers.d/zz-opensu-ring-lights, which lets the user pkexec ran it for run that helper and
# nothing else. The helper takes only a six-digit colour and writes only the controller's lights.

here=$(cd "$(dirname "$0")" && pwd)
. "$here/root-install.sh"

helper=/usr/local/libexec/opensu-ring-lights
rule=/etc/sudoers.d/zz-opensu-ring-lights

place 0755 "$here/opensu-ring-lights" "$helper"
sudoers_rule "$rule" "$user may set the stick lights' colour." "$user ALL=(root) NOPASSWD: $helper"
relabel "$helper" "$rule"

echo "opensu-install: done"

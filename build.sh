#!/usr/bin/env bash
# Build the shell.
#
# The webkit2_41 build tag is required: Fedora 44 ships webkit2gtk-4.1 and no
# longer provides the 4.0 pkg-config file that Wails defaults to.
set -euo pipefail

cd "$(dirname "$0")"
export PATH="$PATH:$HOME/go/bin"

wails build -tags webkit2_41 -o build/bin/iideck "$@"
echo "built build/bin/iideck.AppDir/usr/bin/iideck"
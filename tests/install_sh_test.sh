#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the distribution detection in scripts/install.sh without touching the
# system: a fake os-release, a fake PATH, and FONER_DETECT_ONLY=1.
#
#   sh tests/install_sh_test.sh scripts/install.sh

set -eu
SCRIPT=${1:?path to install.sh}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
FAILED=0

# Only sh and coreutils on the PATH, so no real package manager is visible.
mkdir -p "$WORK/bin"
for tool in sh uname id cat printf sed head command; do
    p=$(command -v "$tool" 2>/dev/null) && [ -f "$p" ] && ln -sf "$p" "$WORK/bin/$tool"
done

run() { # name, os-release body, extra PATH dir, expected exit, expected text
    printf '%s\n' "$2" > "$WORK/os-release"
    out=$(PATH="$3:$WORK/bin" FONER_OS_RELEASE="$WORK/os-release" FONER_DETECT_ONLY=1 \
          sh "$SCRIPT" 2>&1) && rc=0 || rc=$?
    if [ "$rc" -eq "$4" ] && printf '%s' "$out" | grep -q -- "$5"; then
        printf 'ok   %s\n' "$1"
    else
        printf 'FAIL %s (exit %s)\n%s\n' "$1" "$rc" "$out"; FAILED=1
    fi
}

run "derivative via ID_LIKE" 'ID=cachyos
ID_LIKE=arch' "" 0 "family=arch"

run "ubuntu via ID" 'ID=ubuntu' "" 0 "family=debian"

# No known ID, but zypper on the PATH.
mkdir -p "$WORK/suse"; printf '#!/bin/sh\n' > "$WORK/suse/zypper"; chmod +x "$WORK/suse/zypper"
run "unknown ID, zypper on PATH" 'ID=somesuse' "$WORK/suse" 0 "family=suse"

run "unknown, no package manager" 'ID=voidish' "" 1 "FONER_SKIP_PACKAGES=1"

out=$(PATH="$WORK/bin" FONER_OS_RELEASE="$WORK/os-release" FONER_DETECT_ONLY=1 \
      FONER_SKIP_PACKAGES=1 sh "$SCRIPT" 2>&1) && rc=0 || rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q "family=none"; then
    printf 'ok   unknown with FONER_SKIP_PACKAGES=1 continues\n'
else
    printf 'FAIL unknown with FONER_SKIP_PACKAGES=1 (exit %s)\n%s\n' "$rc" "$out"; FAILED=1
fi

run "no os-release at all, no package manager" '' "" 1 "Unrecognised distribution"

exit $FAILED

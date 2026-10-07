#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds and installs Foner.
#
#   curl -fsSLO https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh
#   less install.sh && sh install.sh
#
# Refuses rather than guesses. An unknown distribution, a missing package, a
# Qt that is too old, a failed build: each stops here with a message saying
# what to do, because a dialler that half-installs is worse than one that
# never started. Nothing is installed until every dependency is present.
#
# FONER_PREFIX=~/.local     install without root
# FONER_YES=1               do not ask before installing packages
# FONER_REF=<tag or branch> build this ref instead of main
# FONER_SKIP_PACKAGES=1     you installed the dependencies yourself; build only
# FONER_OS_RELEASE=<file>   read this instead of /etc/os-release (tests)
# FONER_DETECT_ONLY=1       print the distribution family and stop (tests)

set -eu

REPO=https://github.com/AminBlg/foner.git
# main, so a fix reaches new installs without a new release. CI and the tests
# this script runs before installing guard against a bad commit. Set FONER_REF
# to a tag to build a fixed release instead.
REF=${FONER_REF:-main}
QT_MIN_MAJOR=6
QT_MIN_MINOR=5

say()  { printf '%s\n' "$*"; }
step() { printf '\n==> %s\n' "$*"; }
die()  { printf '\nfoner: %s\n' "$*" >&2; exit 1; }

need() { command -v "$1" >/dev/null 2>&1 || die "$2"; }

# ---------------------------------------------------------------- environment

[ "$(uname -s)" = Linux ] || die "Foner talks to PipeWire and BlueZ over D-Bus, so it is Linux only.
Found: $(uname -s)."

case "$(uname -m)" in
    x86_64|aarch64) ;;
    *) die "Untested on $(uname -m). Build it by hand if you want to try:
  git clone $REPO && cd foner && cmake -S . -B build && cmake --build build" ;;
esac

# Detection alone touches nothing, and CI runs its tests as root.
if [ "$(id -u)" -eq 0 ] && [ "${FONER_DETECT_ONLY:-0}" != 1 ]; then
    die "Do not run this as root. It asks for sudo only for the
package install and the final copy, so the build stays yours."
fi

# ---------------------------------------------------------------- distribution

OS_RELEASE=${FONER_OS_RELEASE:-/etc/os-release}
ID= ID_LIKE= PRETTY_NAME=
if [ -r "$OS_RELEASE" ]; then
    # shellcheck disable=SC1090
    . "$OS_RELEASE"
fi

# ID_LIKE is what makes derivatives work without naming every one of them.
FAMILY=
for id in ${ID:-} ${ID_LIKE:-}; do
    case "$id" in
        arch)                 FAMILY=arch;   break ;;
        debian|ubuntu)        FAMILY=debian; break ;;
        fedora|rhel|centos)   FAMILY=fedora; break ;;
        opensuse*|suse)       FAMILY=suse;   break ;;
    esac
done

# Some derivatives set neither a known ID nor ID_LIKE. The package manager on
# the PATH still says which package names apply.
if [ -z "$FAMILY" ]; then
    if   command -v pacman  >/dev/null 2>&1; then FAMILY=arch
    elif command -v apt-get >/dev/null 2>&1; then FAMILY=debian
    elif command -v dnf     >/dev/null 2>&1; then FAMILY=fedora
    elif command -v zypper  >/dev/null 2>&1; then FAMILY=suse
    fi
fi

DEPS_HELP="Foner needs Qt 6.5, extra-cmake-modules, and these KDE Frameworks 6.8 packages
with their development files: Kirigami, KCoreAddons, KDBusAddons, KI18n,
KContacts, KNotifications, KConfig, KStatusNotifierItem, KItemModels.
Calls also need PipeWire 1.4 or newer and BlueZ at run time."

if [ -n "$FAMILY" ]; then
    say "Distribution: ${PRETTY_NAME:-${ID:-unknown}}  (handled as $FAMILY)"
elif [ "${FONER_SKIP_PACKAGES:-0}" = 1 ]; then
    say "Distribution: ${PRETTY_NAME:-${ID:-unknown}}  (unrecognised; building with what is installed)"
else
    die "Unrecognised distribution: ${PRETTY_NAME:-${ID:-unknown}}.

The script knows the package names for Arch, Debian, Ubuntu, Fedora, and
openSUSE and their derivatives. On anything else, install the dependencies
yourself and run the script again with FONER_SKIP_PACKAGES=1, which skips
the package step and lets cmake report anything still missing.

$DEPS_HELP

Nothing has been installed."
fi

if [ "${FONER_DETECT_ONLY:-0}" = 1 ]; then
    say "family=${FAMILY:-none}"
    exit 0
fi

# ---------------------------------------------------------------- packages

if [ "${FONER_SKIP_PACKAGES:-0}" = 1 ]; then
    step "Skipping the package install (FONER_SKIP_PACKAGES=1)"
else

case "$FAMILY" in
arch)
    INSTALL="sudo pacman -S --needed --noconfirm"
    # qt6-tools carries LinguistTools, which CMakeLists requires as a Qt6
    # component. qt6-base does not provide it, and its absence stops the
    # configure step rather than the package install.
    PKGS="base-devel cmake extra-cmake-modules git qt6-base qt6-declarative qt6-tools \
kirigami kcoreaddons kdbusaddons ki18n kcontacts knotifications kconfig \
kstatusnotifieritem kitemmodels"
    ;;
debian)
    INSTALL="sudo apt-get install -y"
    PKGS="build-essential cmake extra-cmake-modules git qt6-base-dev \
qt6-declarative-dev qt6-tools-dev qt6-l10n-tools libkirigami-dev libkf6coreaddons-dev \
libkf6dbusaddons-dev libkf6i18n-dev libkf6contacts-dev \
libkf6notifications-dev libkf6config-dev libkf6statusnotifieritem-dev \
libkf6itemmodels-dev"
    ;;
fedora)
    INSTALL="sudo dnf install -y"
    PKGS="gcc-c++ cmake extra-cmake-modules git qt6-qtbase-devel \
qt6-qtdeclarative-devel qt6-qttools-devel kf6-kirigami-devel kf6-kcoreaddons-devel \
kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kcontacts-devel \
kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel \
kf6-kitemmodels-devel"
    ;;
suse)
    INSTALL="sudo zypper install -y"
    PKGS="gcc-c++ cmake kf6-extra-cmake-modules git qt6-base-devel \
qt6-declarative-devel qt6-linguist-devel kf6-kirigami-devel kf6-kcoreaddons-devel \
kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kcontacts-devel \
kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel \
kf6-kitemmodels-devel"
    ;;
esac

need sudo "sudo is needed to install packages and is not present."

# Ubuntu 24.04 carries KDE Frameworks 5 and no KF6 development packages at
# all, so the build cannot succeed there whatever the package names are.
# Established by asking a container, and checked here before the system is
# touched rather than after a wall of package-manager errors.
if [ "$FAMILY" = debian ]; then
    sudo apt-get update >/dev/null 2>&1 || true
    if ! apt-cache show libkf6coreaddons-dev >/dev/null 2>&1; then
        die "This release carries KDE Frameworks 5, and Foner needs Frameworks 6.
${PRETTY_NAME:-This distribution} has no libkf6 development packages, so the
build cannot succeed here whatever is installed. Debian 13, or Ubuntu 25.04
and newer, will work.

Nothing has been installed."
    fi
fi

# RHEL 9 and its rebuilds are in the same position: no KF6 packages at all.
if [ "$FAMILY" = fedora ] && ! dnf -q info kf6-kcoreaddons-devel >/dev/null 2>&1; then
    die "This release has no KDE Frameworks 6 packages, so the build cannot
succeed here. ${PRETTY_NAME:-This distribution} carries Frameworks 5 or none;
Fedora 40 and newer will work.

Nothing has been installed."
fi

step "Installing build dependencies"
say "  $INSTALL $PKGS"
if [ "${FONER_YES:-0}" != 1 ] && [ -t 0 ]; then
    printf 'Continue? [Y/n] '
    read -r reply
    case "$reply" in [Nn]*) die "Stopped at your request." ;; esac
fi

[ "$FAMILY" = debian ] && sudo apt-get update
# Unquoted on purpose: PKGS is a word list, not one argument.
# shellcheck disable=SC2086
$INSTALL $PKGS || die "The package manager refused. Nothing has been installed.
Read its output above: usually a package under a different name on this release.
If you can install the equivalents yourself, run again with FONER_SKIP_PACKAGES=1."

fi

# ---------------------------------------------------------------- versions

need cmake "cmake is missing even after the package install, which should not happen."
need git   "git is missing even after the package install, which should not happen."

QT_VER=$(qmake6 -query QT_VERSION 2>/dev/null \
    || pkg-config --modversion Qt6Core 2>/dev/null \
    || echo '')
if [ -n "$QT_VER" ]; then
    QT_MAJOR=${QT_VER%%.*}
    QT_REST=${QT_VER#*.}
    QT_MINOR=${QT_REST%%.*}
    if [ "$QT_MAJOR" -lt "$QT_MIN_MAJOR" ] \
       || { [ "$QT_MAJOR" -eq "$QT_MIN_MAJOR" ] && [ "$QT_MINOR" -lt "$QT_MIN_MINOR" ]; }; then
        die "Qt $QT_VER is too old. Foner needs $QT_MIN_MAJOR.$QT_MIN_MINOR or newer.
This usually means the distribution release is older than the app."
    fi
    say "Qt $QT_VER"
fi

# ---------------------------------------------------------------- build

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

step "Fetching the source"
git clone --depth 1 --branch "$REF" "$REPO" "$WORK/foner" \
    || die "Could not clone $REPO at $REF. Check the network and the name of the ref."
cd "$WORK/foner"

step "Building"
PREFIX=${FONER_PREFIX:-/usr/local}
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    || die "cmake could not configure the build. Its message above names the
package that is missing; the list here does not cover this release."
cmake --build build -j"$(nproc 2>/dev/null || echo 2)" \
    || die "The build failed. The output above is the real reason."

step "Checking the build"
ctest --test-dir build --output-on-failure \
    || die "The tests failed on this machine, so the build is not trustworthy
and nothing has been installed."

# ---------------------------------------------------------------- install

step "Installing to $PREFIX"
case "$PREFIX" in
    "$HOME"*) cmake --install build ;;
    *)        sudo cmake --install build ;;
esac

# The build directory is deleted on exit, and cmake writes its list of
# installed files there. Keep the list so the install can be removed later.
MANIFEST_DIR=${XDG_DATA_HOME:-$HOME/.local/share}/foner
mkdir -p "$MANIFEST_DIR"
cp build/install_manifest.txt "$MANIFEST_DIR/install_manifest.txt"

# A desktop session takes its PATH from the systemd user manager, not from a
# shell profile, so ~/.local/bin can be absent there even when the terminal
# finds it. That produces "Could not find the program 'foner'" from the
# launcher while the terminal runs it perfectly.
case "$PREFIX" in
"$HOME"*)
    if ! systemctl --user show-environment 2>/dev/null | grep -q "^PATH=.*$PREFIX/bin"; then
        say ""
        say "$PREFIX/bin is not on the session PATH, so the launcher will not find"
        say "Foner even though your terminal will. Add it once:"
        say ""
        say "  mkdir -p ~/.config/environment.d"
        say "  echo 'PATH=\$HOME/.local/bin:\$PATH' > ~/.config/environment.d/local-bin.conf"
        say ""
        say "Then log out and back in."
    fi
    ;;
esac

step "Done"
say "Run it with: foner"
say "To remove it later, delete the files listed in $MANIFEST_DIR/install_manifest.txt."
say ""

# The telephony D-Bus interface arrived in PipeWire 1.4. Older releases build
# and run Foner but never show a phone, so say so here rather than let the
# window say "no phone connected" with no hint why.
# Output looks like "Linked with libpipewire 1.6.8" on its last line.
PW_VER=$(pipewire --version 2>/dev/null | sed -n 's/^Linked with libpipewire //p' | head -n1)
case "$PW_VER" in
    0.*|1.0.*|1.1.*|1.2.*|1.3.*)
        say "PipeWire $PW_VER is installed. Calls need PipeWire 1.4 or newer, which"
        say "carries the telephony module. Foner will start but report no phone until"
        say "this distribution ships PipeWire 1.4." ;;
    "")
        say "PipeWire was not found on the PATH. Foner needs PipeWire 1.4 or newer with"
        say "its telephony module, and BlueZ, to reach a phone." ;;
    *)
        say "PipeWire $PW_VER. Pair a phone over Bluetooth and Foner will find it." ;;
esac

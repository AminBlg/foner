#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds and installs Foner.
#
#   curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | sh
#
# Refuses rather than guesses. An unknown distribution, a missing package, a
# Qt that is too old, a failed build: each stops here with a message saying
# what to do, because a dialler that half-installs is worse than one that
# never started. Nothing is installed until every dependency is present.
#
# FONER_PREFIX=~/.local   install without root
# FONER_YES=1             do not ask before installing packages

set -eu

REPO=https://github.com/AminBlg/foner.git
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

[ "$(id -u)" -eq 0 ] && die "Do not run this as root. It asks for sudo only for the
package install and the final copy, so the build stays yours."

# ---------------------------------------------------------------- distribution

[ -r /etc/os-release ] || die "No /etc/os-release, so the distribution cannot be
identified. Install the dependencies yourself and run: cmake -S . -B build"
# shellcheck disable=SC1091
. /etc/os-release

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
[ -n "$FAMILY" ] || die "Unrecognised distribution: ${PRETTY_NAME:-${ID:-unknown}}.

Foner needs Qt 6 and these KDE frameworks: Kirigami, KCoreAddons, KDBusAddons,
KI18n, KContacts, KNotifications, KConfig, KStatusNotifierItem. Install them
with your package manager, then:

  git clone $REPO && cd foner
  cmake -S . -B build && cmake --build build && sudo cmake --install build"

say "Distribution: ${PRETTY_NAME:-$ID}  (handled as $FAMILY)"

# ---------------------------------------------------------------- packages

case "$FAMILY" in
arch)
    INSTALL="sudo pacman -S --needed --noconfirm"
    # qt6-tools carries LinguistTools, which CMakeLists requires as a Qt6
    # component. qt6-base does not provide it, and its absence stops the
    # configure step rather than the package install.
    PKGS="base-devel cmake extra-cmake-modules git qt6-base qt6-declarative qt6-tools \
kirigami kcoreaddons kdbusaddons ki18n kcontacts knotifications kconfig \
kstatusnotifieritem"
    ;;
debian)
    INSTALL="sudo apt-get install -y"
    PKGS="build-essential cmake extra-cmake-modules git qt6-base-dev \
qt6-declarative-dev qt6-tools-dev qt6-l10n-tools libkirigami-dev libkf6coreaddons-dev \
libkf6dbusaddons-dev libkf6i18n-dev libkf6contacts-dev \
libkf6notifications-dev libkf6config-dev libkf6statusnotifieritem-dev"
    ;;
fedora)
    INSTALL="sudo dnf install -y"
    PKGS="gcc-c++ cmake extra-cmake-modules git qt6-qtbase-devel \
qt6-qtdeclarative-devel qt6-qttools-devel kf6-kirigami-devel kf6-kcoreaddons-devel \
kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kcontacts-devel \
kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel"
    ;;
suse)
    INSTALL="sudo zypper install -y"
    PKGS="gcc-c++ cmake kf6-extra-cmake-modules git qt6-base-devel \
qt6-declarative-devel qt6-linguist-devel kf6-kirigami-devel kf6-kcoreaddons-devel \
kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kcontacts-devel \
kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel"
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
Read its output above: usually a package under a different name on this release."

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
git clone --depth 1 "$REPO" "$WORK/foner" \
    || die "Could not clone $REPO. Check the network."
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
say ""
say "Foner needs PipeWire with its telephony module, BlueZ, and a phone paired"
say "over Bluetooth. Without those it starts and reports that no phone is"
say "connected, which is the correct behaviour rather than a fault."

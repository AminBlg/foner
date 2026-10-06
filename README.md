> [!WARNING]
> **Foner is under active development** and not yet stable. It runs every day against one phone on one desktop, and nothing else yet. If you want to know when the next release lands, give the repo a **Star** ⭐ and **Watch** 👀 it.

<p align="center">
    <img src="packaging/icons/org.unscale.foner.svg" width="138" alt="Foner"/>
</p>

<h1 align="center">Foner</h1>
<p align="center"><strong>📞 A native KDE dialler for the phone in your pocket, written in C++ and Kirigami<br/>Make and receive calls on your desktop through a phone paired over Bluetooth</strong></p>

<div align="center">
    <a href="https://github.com/AminBlg/foner/actions/workflows/build.yml" target="_blank">
    <img alt="Build" src="https://github.com/AminBlg/foner/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/AminBlg/foner/commits" target="_blank">
    <img alt="GitHub commit activity" src="https://img.shields.io/github/commit-activity/m/AminBlg/foner?style=flat"></a>
    <a href="LICENSE" target="_blank">
    <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg?style=flat"></a>
    <a href="https://www.qt.io/" target="_blank">
    <img alt="Qt 6" src="https://img.shields.io/badge/Qt-6.5%2B-41cd52.svg?style=flat"></a>
    <a href="https://develop.kde.org/products/frameworks/" target="_blank">
    <img alt="KDE Frameworks" src="https://img.shields.io/badge/KDE%20Frameworks-6.8%2B-1d99f3.svg?style=flat"></a>
</div>

<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/keypad-dark.png">
  <img src="docs/screenshots/keypad.png" width="270" alt="The keypad, with the connected phone named along the top">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/contacts-dark.png">
  <img src="docs/screenshots/contacts.png" width="270" alt="The contacts, with one expanded to show its two numbers">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/in-call-dark.png">
  <img src="docs/screenshots/in-call.png" width="270" alt="A call in progress, with mute, volume and End">
</picture>
</p>

> **Tired of reaching for the phone every time it rings? Try Foner.**

Runs on Linux with Plasma, or any desktop with PipeWire and BlueZ.

## Install

> [!IMPORTANT]
> Foner needs **PipeWire 1.4 or newer with its telephony module** and **BlueZ**. The phone must be paired and connected over the hands-free profile. Without those it starts and reports that no phone is connected.

One command on any supported distribution. The install script picks the package manager, installs the dependencies, builds, tests, and installs to `/usr/local`. It stops with a reason rather than guessing, and installs nothing until every dependency is present.

```sh
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | sh
```

Into `~/.local` instead, without root:

```sh
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | FONER_PREFIX=~/.local sh
```

The script knows the package names for Arch, Debian, Ubuntu, Fedora, and openSUSE. It recognises their derivatives by the `ID_LIKE` field or by the package manager on the path. On any other distribution it stops, prints the dependency list, and installs nothing. Install the dependencies yourself, then run it again with `FONER_SKIP_PACKAGES=1`. It then builds with what is there:

```sh
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | FONER_SKIP_PACKAGES=1 sh
```

At the end the script reports the installed PipeWire version and says whether it is new enough for calls.

### Supported distributions

Foner needs Qt 6.5, KDE Frameworks 6.8, and PipeWire 1.4. The build needs the first two. Calls need the third.

| Distribution | Status |
|---|---|
| Arch Linux, CachyOS, EndeavourOS, Manjaro | Works |
| Fedora 42 and newer | Works |
| Ubuntu and Kubuntu 25.10 and newer | Works |
| Debian 13 | Works |
| openSUSE Tumbleweed, Leap 16.0 | Works |
| Ubuntu and Kubuntu 25.04 | Builds, but PipeWire 1.2 has no telephony module, so no calls |
| KDE neon | Builds, but PipeWire from the Ubuntu 24.04 base has no telephony module, so no calls |
| Ubuntu 24.04, Debian 12, RHEL 9, openSUSE Leap 15.6 | No KDE Frameworks 6, so no build |

Only Arch with Plasma ran against a real phone. The others are judged from the package versions they ship.

## Build

Install the dependencies with your package manager, then run CMake.

**Arch Linux**

```sh
sudo pacman -S --needed base-devel cmake extra-cmake-modules git qt6-base qt6-declarative qt6-tools \
  kirigami kcoreaddons kdbusaddons ki18n kcontacts knotifications kconfig kstatusnotifieritem kitemmodels
```

**Debian and Ubuntu**

```sh
sudo apt-get install build-essential cmake extra-cmake-modules git qt6-base-dev qt6-declarative-dev \
  qt6-tools-dev qt6-l10n-tools libkirigami-dev libkf6coreaddons-dev libkf6dbusaddons-dev libkf6i18n-dev \
  libkf6contacts-dev libkf6notifications-dev libkf6config-dev libkf6statusnotifieritem-dev libkf6itemmodels-dev
```

**Fedora**

```sh
sudo dnf install gcc-c++ cmake extra-cmake-modules git qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qttools-devel kf6-kirigami-devel kf6-kcoreaddons-devel kf6-kdbusaddons-devel kf6-ki18n-devel \
  kf6-kcontacts-devel kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel kf6-kitemmodels-devel
```

**openSUSE**

```sh
sudo zypper install gcc-c++ cmake kf6-extra-cmake-modules git qt6-base-devel qt6-declarative-devel \
  qt6-linguist-devel kf6-kirigami-devel kf6-kcoreaddons-devel kf6-kdbusaddons-devel kf6-ki18n-devel \
  kf6-kcontacts-devel kf6-knotifications-devel kf6-kconfig-devel kf6-kstatusnotifieritem-devel kf6-kitemmodels-devel
```

Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

[docs/demo-contacts.vcf](docs/demo-contacts.vcf) holds invented contacts, so the search and the contact list can be seen with no phone connected.

## Features

- Dial a number, answer a call, and end a call.
- Read the contacts and the call history from the phone on connect, and keep them on disk.
- Import contacts from a vCard file, and export them back out to one.
- Open a `tel:` link from another application.

**Calls**

- Hold one call and answer another. Swap between two calls. Join two calls together.
- Mute the microphone and set the speaker level. Send keypad tones during a call.
- Show where the call audio is, and pull it onto this computer.
- Send a service code such as `*710#`. The network answers on the phone itself.

**Contacts**

- Search by name or by number. Digits also search names, the way a phone keypad does: `254` finds "Ali".
- Accents fold, so `Bechir` finds "Béchir".
- Sort by name or by who you called last.
- Expand a contact to see every number it holds.

**Desktop**

- Stay in the system tray with the window closed, so an incoming call still reaches you.
- Show a desktop notification for an incoming call, with Answer and Decline buttons.
- Three tabs: a keypad, the contacts, and the recent calls.

## How it works

Foner talks to PipeWire over the `org.pipewire.Telephony` D-Bus interface. PipeWire talks to the phone over the Bluetooth hands-free profile. The phone talks to the mobile network. Foner never touches the network itself.

[docs/M0-selfio-notes.md](docs/M0-selfio-notes.md) records what the live interface does, and where it differs from its own introspection data.

## Usage

```sh
foner
```

Connect your phone. When it connects, Foner reads the phonebook and the call history.

## Status

See [docs/STATUS.md](docs/STATUS.md).

Dialling, answering, hanging up, the contacts and the call history, the search, the notifications, and the tray are used every day against a real phone. Service codes, two calls at once, and vCard export are built but not proved against one.

## Developing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Acknowledgments

- [PipeWire](https://pipewire.org/) for the telephony module that makes the hands-free profile reachable from a desktop application
- [BlueZ](http://www.bluez.org/) for the Bluetooth stack
- [KDE Frameworks](https://develop.kde.org/products/frameworks/) and [Kirigami](https://develop.kde.org/frameworks/kirigami/) for the user interface

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

---

## Beyond KDE Connect

KDE Connect tells you that the phone is ringing. Foner picks up. It answers and dials from the desktop, carries the call audio through this computer, and keeps the phonebook and the call history on disk. No account, no cloud, no telemetry. The call never leaves the phone and the mobile network.

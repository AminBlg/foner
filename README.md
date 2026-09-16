> [!WARNING]
> **Foner is under active development** and not yet stable. It runs every day against one phone on one desktop, and nothing else yet. If you want to know when the first release lands, give the repo a **Star** ⭐ and **Watch** 👀 it.

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

---

## Beyond KDE Connect

KDE Connect tells you that the phone is ringing. Foner picks up. Things Foner does that KDE Connect will not:

- **Answer on the desktop.** Answer, decline, hold, swap, and end calls from the window, the tray, or the notification.
- **Carry the audio.** The call comes through the speakers and the microphone of this computer. The phone stays in your pocket.
- **Dial from a keypad.** A real dial pad with keypad tones, not a confirmation box for a `tel:` link.
- **Keep the phonebook.** The contacts and the call history are read from the phone over Bluetooth and kept on disk.
- **Stay native.** C++ and Kirigami, no account, no cloud, no telemetry. The call never leaves the phone and the mobile network.

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

## Install

> [!IMPORTANT]
> Foner needs **PipeWire with its telephony module**, **BlueZ**, and a phone that is paired and connected over the hands-free profile. Without those it starts and reports that no phone is connected.

Needs Qt 6.5 and KDE Frameworks 6.8, so Ubuntu 24.04 and RHEL 9 will not work.

### Arch Linux

```sh
cd packaging && makepkg -si
```

### Any distribution

The install script installs the dependencies, builds, tests, and installs to `/usr/local`. It stops with a reason rather than guessing, and installs nothing until every dependency is present.

```sh
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | sh
```

Into `~/.local` instead, without root:

```sh
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | FONER_PREFIX=~/.local sh
```

### From source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

[docs/demo-contacts.vcf](docs/demo-contacts.vcf) holds invented contacts, so the search and the contact list can be seen with no phone connected.

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

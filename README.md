<div align="center">

<img src="packaging/icons/org.unscale.foner.svg" width="96" alt="">

# Foner

**Make and receive phone calls through a phone paired over Bluetooth.**

[![Build](https://github.com/AminBlg/foner/actions/workflows/build.yml/badge.svg)](https://github.com/AminBlg/foner/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Qt 6](https://img.shields.io/badge/Qt-6.5%2B-41cd52.svg)](https://www.qt.io/)
[![KDE Frameworks 6.8+](https://img.shields.io/badge/KDE%20Frameworks-6.8%2B-1d99f3.svg)](https://develop.kde.org/products/frameworks/)

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

</div>

The call stays on the phone and on the mobile network. Foner sends the commands and
carries the audio, so the phone can stay in your pocket.

It is a KDE application, written with Qt 6 and Kirigami.

## Install

```
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | sh
```

Installs the dependencies, builds, tests, and installs to `/usr/local`. It stops with a
reason rather than guessing, and installs nothing until every dependency is present.

Needs Qt 6.5 and KDE Frameworks 6.8, so Ubuntu 24.04 and RHEL 9 will not work.

Into `~/.local` instead, without root:

```
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | FONER_PREFIX=~/.local sh
```

On Arch, `cd packaging && makepkg -si` builds a real package.

## Run it

```
foner
```

Connect your phone. Foner reads the phonebook and the call history when it connects.

Foner needs PipeWire with its telephony module, BlueZ, and a paired phone. Without those
it starts and reports that no phone is connected.

## What it does

- Dial a number, answer a call, and end a call.
- Search the contacts by name or by number. Digits also search names, the way a phone
  keypad does: `254` finds "Ali". Accents fold, so `Bechir` finds "Béchir".
- Sort the contacts by name or by who you called last.
- Read the contacts and the call history from the phone, and keep them on disk.
- Import contacts from a vCard file, and export them back out to one.
- Hold one call and answer another. Swap between two calls. Join two calls together.
- Mute the microphone and set the speaker level. Send keypad tones during a call.
- Show where the call audio is, and pull it onto this computer.
- Send a service code such as `*710#`. The network answers on the phone itself.
- Open a `tel:` link from another application.
- Stay in the system tray with the window closed, so an incoming call still reaches you.
- Show a desktop notification for an incoming call, with Answer and Decline buttons.

Three tabs: a keypad, the contacts, and the recent calls.

## How it works

Foner talks to PipeWire over the `org.pipewire.Telephony` D-Bus interface. PipeWire
talks to the phone over the Bluetooth hands-free profile. The phone talks to the mobile
network. Foner never touches the network itself.

`docs/M0-selfio-notes.md` records what the live interface does, and where it differs
from its own introspection data.

## Build it yourself

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`docs/demo-contacts.vcf` holds 16 invented contacts, so the search and the contact list
can be seen with no phone connected.

## Status

Dialling, answering, hanging up, the contacts and the call history, the search, the
notifications, and the tray are used every day against a real phone. Service codes, two
calls at once, and vCard export are built but not proved against one.

`docs/STATUS.md` carries the detail. `CONTRIBUTING.md` describes the layout and the
tests.

## License

GPL-3.0-or-later. See `LICENSE`.

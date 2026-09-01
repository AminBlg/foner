# Foner

Foner makes and receives phone calls through a phone that is paired over Bluetooth.
The call stays on the phone and on the mobile network. Foner sends the commands and
carries the audio, so the phone can stay in your pocket.

Foner is a KDE application. It is written with Qt 6 and Kirigami.

## How it works

Foner talks to PipeWire over the `org.pipewire.Telephony` D-Bus interface. PipeWire
talks to the phone over the Bluetooth hands-free profile. The phone talks to the mobile
network.

Foner never touches the network itself. If no phone is connected, Foner shows no device
and cannot dial. It reads the phonebook straight from the phone over Bluetooth PBAP.

## What it does

- Dial a number, answer a call, and end a call.
- Search the contacts by name or by number. Digits also search names, the way a phone
  keypad does: `254` finds "Ali". Accents fold, so `Bechir` finds "Béchir".
- Sort the contacts by name or by who you called last.
- Read the contacts and the call history from the phone. The contacts are kept on disk,
  so a caller still has a name after a restart.
- Import contacts from a vCard file, and export them back out to one.
- Switch between phones from the chip in the corner, which also shows the phone's name
  and its battery level.
- Hold one call and answer another. Swap between two calls. Join two calls together.
- Mute the microphone and set the speaker level. Send keypad tones during a call.
- Send a service code such as `*710#`. The network answers on the phone itself, because
  the hands-free profile carries no channel for the reply.
- Open a `tel:` link from another application.
- Stay in the system tray with the window closed, so an incoming call still reaches you.
- Show a desktop notification for an incoming call, with Answer and Decline buttons.
  Missed calls, failures and the phone connecting also get one.
- Work from the keyboard alone.

The window has three tabs: a keypad, the contacts, and the recent calls. The search
field and the call in progress stay on screen above them, whichever tab is open.

There is a menu bar with File, Device and Help. On a desktop with a global menu, such as
Plasma, it appears in the panel. On a desktop without one it is drawn in the window.
Everything in it is also in the drawer behind the hamburger button, so the two never
disagree.

## Requirements

- PipeWire with telephony support, version 1.4 or later.
- BlueZ. The phonebook also needs `bluez-obex`.
- Qt 6.5 or later, and KDE Frameworks 6.
- A phone that is paired and connected over the hands-free profile (HFP).

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Install

```
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | sh
```

It installs the dependencies, builds, runs the tests, and installs to `/usr/local`.
Nothing is installed until every dependency is present, and it stops with a reason
rather than guessing: an unrecognised distribution, a release carrying KDE Frameworks 5,
a Qt older than 6.5, or a failing test each halt it.

Tested in containers on Arch, Debian 13, Fedora 41 and openSUSE Tumbleweed. Ubuntu 24.04
and RHEL 9 are refused, because they ship Frameworks 5 and cannot build this at all.

Without root, into `~/.local`:

```
curl -fsSL https://raw.githubusercontent.com/AminBlg/foner/main/scripts/install.sh | FONER_PREFIX=~/.local sh
```

Arch users who would rather have a real package:

```
cd packaging && makepkg -si
```

Installing matters beyond convenience. `foner.notifyrc` declares the notification
events, and without it KDE Frameworks drops every notification silently, so a copied
binary has no incoming-call popup at all.

## First run

```
foner
```

After the very first install, restart the desktop shell once:

```
systemctl --user restart plasma-plasmashell
```

The shell caches the result of an icon lookup for as long as it runs. It was started
before Foner's icon existed, so until it restarts the icon stays blank.

Then connect your phone and press the button beside the contact count to read the
phonebook.

## Try it without a phone

`docs/demo-contacts.vcf` holds fifteen made-up contacts. Open it with the file button on
the Contacts tab. The contacts, the search and the keypad search all work with no phone
paired.

## Tests

```
ctest --test-dir build --output-on-failure
```

29 cases across two test programs, plus a check that the AppStream metadata is valid.
They cover the vCard parser, the call-history parser, the number matching, the keypad
and accent search, which property carries the caller's number, which strings are service
codes, and the grace window that decides when a call ended. No phone and no D-Bus
connection are needed.

`build/bin/fonerprobe` is a tool and not a test. It prints the live D-Bus state and it
needs a connected phone.

## Troubleshooting

**No notifications at all.** `foner.notifyrc` is not installed, which happens when the
binary was copied into place rather than installed.

**The icon is blank.** Restart the desktop shell once, as described in First run.

**No phone in the list.** The phone is paired but not connected over the hands-free
profile. Connect it, then check that `busctl --user tree org.pipewire.Telephony` shows an
audio gateway.

**A caller shows a number where a name belongs.** The phonebook has not been read yet.
Press the refresh button on the Contacts tab. Foner keeps it after that.

Foner writes its messages to the journal and not to the terminal:

```
journalctl --user -n 50 --no-pager
```

For more than warnings, run it once with its logging turned on. This prints every
call state change, every notification, and every D-Bus error:

```
QT_LOGGING_RULES="foner.*=true" foner
```

The categories are `foner.telephony`, `foner.contacts`, `foner.bluez`,
`foner.notifications` and `foner.tray`, so one area can be followed on its own.
Please include this output in a bug report.

## Status

Version 0.1.0. Dialling, answering, hanging up, contacts and call history over Bluetooth,
search, notifications and the tray are all used daily against a real phone. Service
codes, two calls at once, and vCard export are built but have not been proved against
one.

`docs/STATUS.md` carries the current detail: what is proved, what is merely built, what
the gateway cannot do, and what is left. It is kept up to date; this paragraph is not the
place to look.

## Notes on the D-Bus interface

`docs/M0-selfio-notes.md` records what the live interface does, and where it differs
from its own introspection data. Three findings shaped the code:

- `GetManagedObjects` does not list call objects. It lists only the audio gateway.
- `Hangup` on `org.pipewire.Telephony.Call1` returns an error. Call control uses
  `org.ofono.VoiceCall` instead.
- `IncomingLine` is the called line and the network leaves it empty. The caller's number
  is in `LineIdentification`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

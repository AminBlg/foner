<!--
SPDX-FileCopyrightText: 2026 AminBlg
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Contributing

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

You need Qt 6, KDE Frameworks 6, and the development files for PipeWire and BlueZ.
The build must stay free of compiler warnings.

## Tests

```
ctest --test-dir build --output-on-failure
```

The tests need no phone and no D-Bus connection. They cover the vCard parser, the
call-history parser, the number matching, the keypad search, and the grace window
that decides when a call has ended. Add a test with any change to those parts.

## Checks before a pull request

```
/usr/lib/qt6/bin/qmllint src/app/qml/*.qml -I /usr/lib/qt6/qml
desktop-file-validate packaging/org.unscale.foner.desktop
appstreamcli validate --no-net packaging/org.unscale.foner.metainfo.xml
```

`qmllint` reports `[unqualified]` for the context properties and for the
translation functions. Any other category is a defect.

## Logging

All logging is off by default. Turn it on for one run:

```
QT_LOGGING_RULES="foner.*=true" build/bin/foner
```

Categories are declared in `src/core/logging.h`. Warnings are always on, so a
persistent failure reaches the journal without anyone reproducing it with a flag
set. Add a `qCDebug` wherever a future crash report would want one, and a
`qCWarning` on any D-Bus reply carrying an error.

## Changes that need a phone

The D-Bus behaviour of the audio gateway is not what its introspection data says.
`docs/M0-selfio-notes.md` records what the live interface does. Read it before you
change anything in `src/core/telephony/`.

These parts can only be tested against a paired phone:

- Placing, answering, and ending a call.
- Two calls at once: hold, swap, and join.
- Keypad tones during a call.
- Mute, and the speaker level.
- The contact and call-history import over Bluetooth.

`build/bin/fonerprobe` prints the live D-Bus state. It is a tool, not a test.

## Licensing

Every new file needs an SPDX header:

```
// SPDX-FileCopyrightText: 2026 Your Name
// SPDX-License-Identifier: GPL-3.0-or-later
```

## Do not commit

Real phone numbers, Bluetooth addresses, host names, or home directory paths. The
sample numbers start `+21355000` in international form or `055000` in national form,
and the sample Bluetooth address is `AA:BB:CC:DD:EE:FF`. The numbers are invented,
but 055 is a mobile range that Algeria assigns to Ooredoo. A real subscriber can
hold any of them, so never dial one.

# Foner: project status

Written 2026-09-02, and updated 2026-10-06 for the `v0.1.0` tag. A snapshot of what
works, what does not, and what is known about the interface underneath. The README
describes the application. This describes its condition.

The commit count is low because the history was flattened before publication. A commit
message carried the author's phone Bluetooth address, and a scan of the tracked files
does not catch that.

## Where it stands

Foner is a working desktop dialler. Calls are placed, answered and ended against a real
phone, contacts and history arrive over Bluetooth, and the window survives being closed
because the tray keeps it listening. It is used daily by its author on Arch with Plasma.

It is one person's application. It ran against two handsets, one Android phone and one
iPhone, but not against a second desktop or a second distribution.

| | |
|---|---|
| Version | 0.1.0, tagged `v0.1.0` |
| Source | 6,183 lines of C++ and QML |
| Tests | 3 binaries, 141 assertions |
| Requires | Qt 6.5, KDE Frameworks 6.8, and 9 KDE frameworks |
| Repository | `AminBlg/foner` |

## Proved against a real phone

- Dialling, answering and ending a call.
- The incoming-call notification, with Answer and Decline on the notification itself.
- Contacts and call history pulled from the phone over PBAP.
- Contact search by name, by number, by keypad digits, and across accents.
- The menu bar, and the chip naming the connected phone and its battery.
- Reading where the call audio is, and pulling it onto this computer.
- The tray entry, and settings that persist between runs.

## Built, not yet proved

- **Service codes.** `*710#` goes out. No reply came back from a live network. HFP
  carries no channel for one, so the answer can only appear on the handset.
- **Two calls at once**, meaning hold, swap and join. The code paths exist and the gateway
  exposes the methods. The situation did not arise in testing.
- **The About page and vCard export.** Export has a round-trip test covering accents and
  multi-number contacts, but neither ran against a phone.

## Known limits

These are properties of the interface, not defects to be fixed. Each was established by
introspecting a live gateway, and each is recorded so it is not rediscovered.

- **Call audio can be pulled here, not pushed back.**
  `AudioGatewayTransport1` exposes `Activate()` and no `Deactivate`. `RejectSCO` refuses
  the *next* SCO link rather than closing the one already open, so a call whose audio is
  on this machine stays here until it ends. A "move to phone" button existed briefly and
  was removed for promising what the gateway cannot do.
- **A merged call cannot be split again.** The gateway offers no method.
- **One notification at a time.** A second call arriving while the first still rings
  replaces the first notification. Both calls still get a row in the window.
- **`Kirigami.Theme` has no `separatorColor`.** Twenty colour roles, and separator is not
  among them. Binding to it yields `undefined` and draws nothing.
- **`Qt.alpha()` needs Qt 6.11.** The build asks for 6.5, so the code uses `Qt.rgba` with
  the components read off the theme colour instead.
- **`KLocalizedQmlContext` needs Frameworks 6.8.** That class sets the framework floor.
  Every other KDE API the code calls is 6.0 or older.

## Not done

- **Keyboard-only operation is incomplete.** Audited, not yet fixed. The keypad keys are
  unreachable by Tab: `Keypad.qml` sets no focus policy, so all twelve are skipped, and a
  keyboard user has to type into the field instead of operating the visible control. The
  tab chain also dead-ends at the tab bar rather than cycling back, and no shortcut
  reaches the tabs, so switching to Contacts means walking there with Tab. Focus rings
  are the style's default and nothing overrides them, but that has not been confirmed on
  a real display.
- The Flatpak manifest is written and has never been built. It does not list KContacts,
  which the KDE runtime can lack. Building it needs flatpak-builder, which is not
  installed here, and roughly 1.5 GB of KDE SDK, so the question of what else the runtime
  lacks is still open.

## What today's work changed

Twenty-one commits, most of them corrections rather than features.

**Interface**, rebuilt against an agreed mockup: framed keys and a full-width Call bar,
flat lists with hairline separators, missed calls labelled in words as well as colour, a
live call timer, and the phone chip given a row of its own after it was found sitting on
top of the recents reload button.

**A class of bug worth naming.** Connecting `commandFailed` to a visible banner made
every refused command visible, which exposed controls that stayed enabled in states where
their command can only fail: dialling during a call, sending tones to a call that was
merely ringing, merging against a leg that nobody answered, switching phones
mid-call. Five of those were found and fixed. They had all been failing silently for as
long as they had existed.

**Two bugs found by compilers rather than by reading.** An overflowing hex escape in the
contacts test meant the accent round trip passed on the wrong bytes. `-Wall` knew all
along. A binding to a theme role that does not exist drew the phone chip with no
border. `qmllint` knew all along. Both checks now run in CI, where the warning step
previously passed for nothing, because nothing enabled any warnings.

**Since then**, a health ratchet: `scripts/health.sh` measures eight metrics and
`scripts/gate.sh` refuses a regression, both wired into CI and a pre-commit hook. The
audit findings left unverified were resolved - one real and fixed, one real and deleted -
and the enabled-in-impossible-state class was re-checked systematically with a matrix of
every control against every call state rather than waiting for the next one to surface.
It found one more: Swap was offered while the second call was still ringing.

**Then a one-line installer**, and the containers that tested it found four faults that no
local check can find. Three package names were wrong on three distributions. Qt6
LinguistTools was required by CMake and named in no package list. `kitemmodels` was
imported by the QML and declared nowhere at all, because a QML import leaves no trace in
the linker. The framework floor said 6.0 and the code needs 6.8.

Each one was invisible here for the same reason: this machine has every dependency
installed and every version far above the declared floor, so a missing declaration and a
wrong minimum both look exactly like success.

## Four other implementations

Built to judge the interface outside Kirigami's constraints, then set aside. Each carries
its own D-Bus layer ported from the C++. Each passes the same seven behavioural tests,
which are accent folding, T9 search, number normalisation and the vCard round trip.
Each was verified in Rust, JavaScript, Python and Dart.

| | Stack | Verdict |
|---|---|---|
| `foner-tauri` | Rust + webview | Kept. Shares its UI with the Electron build |
| `foner-electron` | Node | Rejected on looks |
| `foner-gtk` | Python, GTK4 + libadwaita | Rejected on looks |
| `foner-flutter` | Dart | Rejected on looks |

Kirigami remains the real application. The exercise settled the question rather than
changing the answer.

## What I would do next

1. **Finish keyboard-only operation.** The audit above says what is wrong. None of it is
   fixed.
2. **Test on more phones.** Every interface fact recorded here comes from one handset,
   and some of them are certainly properties of that handset rather than of HFP.

## Deferred

Ideas raised and not acted on, recorded so they are not raised again as though new.

- A per-row Call button on the keypad suggestion strip. The strip stopped dialling on
  touch so a reader can select and copy a number. A button restores the shortcut without
  the hazard, at the cost of a control on a strip meant to stay compact.
- `AgModel::get` was removed as dead. If a device picker ever needs indexed access the
  way the incoming-call sheet needs it for calls, it comes back.

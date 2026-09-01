# M0 — Live stack proof (org.unscale.foner)

This is the log of the D-Bus session that proved the stack works, taken on
2026-08-09 against a real paired phone. It is kept because several of the findings
below contradict the interface's own introspection data and appear in no
specification, and the code still depends on them. Read it before you change
anything in `src/core/telephony/`.

Status: PASSED. The full relay path works on an Arch / KDE Plasma machine.

## Environment (verified live, 2026-08-09)
- distro: Arch
- PipeWire 1.6.8 (>=1.4 ✓), WirePlumber + pipewire-pulse active as user services
- BlueZ / bluetoothctl 5.87; controller class exposes "Handsfree Audio Gateway"
- Phone: Samsung `da-m32`, BT address `AA:BB:CC:DD:EE:FF`, icon=phone, paired+trusted+connected
  - modalias `bluetooth:v0075p0100d0201` (v0075 = Samsung)
  - exposes Handsfree Audio Gateway + Phonebook Access Server (PBAP ✓) + Message Access Server UUIDs
- PipeWire sees it as `da-m32 [bluez5]` in AG role; `AudioGatewayTransport1.Codec` = 2 (mSBC wideband)

## What works (commands that actually ran)
- Enumerate AG: `busctl --user call org.pipewire.Telephony /org/pipewire/Telephony org.freedesktop.DBus.ObjectManager GetManagedObjects`
  returns a single object `/org/pipewire/Telephony/ag1` with
  `org.pipewire.Telephony.AudioGateway1` (`Address`, `SpeakerVolume`, `MicrophoneVolume`)
  + `org.pipewire.Telephony.AudioGatewayTransport1` (`Codec`, `State`, `RejectSCO`).
- Initiate call:
  `busctl --user call org.pipewire.Telephony /org/pipewire/Telephony/ag1 org.pipewire.Telephony.AudioGateway1 Dial s +213550000001`
  -> phone places a REAL call; transport `State` flips `idle -> active` while engaged, back to `idle` on end.

## Call object (path `/org/pipewire/Telephony/ag1/call1`)
Exposes TWO interfaces (verified via introspect while dialing):
- `org.pipewire.Telephony.Call1`: props `State="dialing"`, `LineIdentification="+213550000001"`,
  `Name`, `IncomingLine`, `Multiparty`; methods `Answer`, `Hangup`
- `org.ofono.VoiceCall` (oFono compat): methods `Answer`, `Hangup`, `GetProperties`; signal `PropertyChanged`

## Critical findings for M1 design
1. **`GetManagedObjects` does NOT list call objects** (only the AG). Track calls via
   `org.ofono.VoiceCallManager` signals `CallAdded`/`CallRemoved` (or ObjectManager
   `InterfacesAdded/Removed`), NOT a managed query. Call paths are `/…/ag1/callN`.
2. **`Hangup` failed on `org.pipewire.Telephony.Call1`** with "Method doesn't exist" even though
   introspect lists it. Plan: build call-control against **`org.ofono.VoiceCall`** as primary;
   re-verify `Call1` at runtime in M1.
3. Hold/conference methods live on the AG interface: `HoldAndAnswer`, `ReleaseAndAnswer`,
   `ReleaseAndSwap`, `SwapCalls`, `CreateMultiparty`, `SendTones`, `HangupAll` (both
   `org.pipewire.Telephony.AudioGateway1` and `org.ofono.VoiceCallManager`).

## M0 follow-up (live, second call: +213550000003)
- `org.ofono.VoiceCall.Hangup` WORKS and ends the call (no error returned; call removed after).
  Re-confirms `Call1.Hangup` was the unreliable one. M1 call-control primary:
  `org.ofono.VoiceCall` for per-call control (Answer/Hangup/GetProperties).
- `org.ofono.VoiceCallManager.GetCalls` DOES enumerate active calls (returns `a{oa{sv}}`
  dict of call properties incl. `State`, `LineIdentification`, `IncomingLine`, `Name`,
  `Multiparty`). Race: returns 0 during the very brief `dialing` registration window.
  M1 should poll GetCalls after any call-affecting action, not rely on one snapshot.
- Call `State` transitions observed: `"dialing"` -> `"active"` -> (Hangup) -> removed.
- `busctl --user call ... org.ofono.VoiceCall.Hangup` printed "Too few arguments" only when
  called redundantly mid-hangup — harmless; treat as already-ended.

## M1-critical: GetCalls is unreliable across transitions (call to +213550000002)
- While a call is progressing (dialing -> ringing), `GetCalls` intermittently returns `a{oa{sv}} 0`
  (empty) and later shows the call, then the call object is removed once the call ends.
- Conclusion for M1: **track calls via D-Bus signals, not polling**:
  - `org.ofono.VoiceCallManager.CallAdded(path, props)` / `CallRemoved(path)` on the AG object
  - and/or `org.freedesktop.DBus.ObjectManager.InterfacesAdded/Removed` at `/org/pipewire/Telephony`
  - Use `GetCalls` only as an initial/refresh snapshot.
- After the call ends, the `callN` object path is removed (GetAll on it fails).

## Notes
- AG transport `State` (idle/active) is the reliable "call engaged" fallback indicator.
- Numbers in this document are synthetic. Do not dial a real number to test
  without asking its owner first.
# APC40 MkII Control Surface Prototype

A native C++17 prototype for banked APC40 MkII controls and bidirectional MIDI
feedback, intended for eventual use from the Nexus6 Arduino sequencer.

The portable `apc40_core` target contains controls, banks, MIDI decoding, and
feedback encoding. RtMidi is used only by the desktop host transport. See
[ROADMAP.md](ROADMAP.md) for the agreed integration plan and remaining work.

## Native Host

The host uses the local copies of RtMidi and foundling under `external/`.

```sh
cmake -S . -B build
cmake --build build --config Debug --target apc40_host list_ports
```

On Windows, run `build/Debug/apc40_host.exe`. The current device discovery looks
for MIDI ports whose names contain `APC40` (matching `APC40 mkII`). Pass another
name substring to the transport constructor when necessary. A failed match
prints the available input and output port names. Windows host builds copy
the RtMidi DLL beside `apc40_host` and `list_ports` when building a shared RtMidi.

For a Windows hardware smoke test, stop any older host process, rebuild, and
check that `list_ports` shows an APC40 input and output before launching the
host:

```powershell
.\build\Debug\list_ports.exe
.\build\Debug\apc40_host.exe
```

Successful startup prints `APC40 MkII connected.` and sends the introduction
before drawing bank LEDs and encoder rings. Play and Record toggle the mock app
state, print the new state, and update their LEDs. Track-knob turns print the
logical bank, track, and value; faders and relative encoder deltas are also
reported. The APC updates its own encoder ring locally.

```cpp
#include "APC40Controller.h"
#include "APC40RtMidiTransport.h"

APC40RtMidiTransport transport;
APC40Controller apc(transport);

// In application startup:
// if (!apc.init()) { ... }
// In the application loop:
// apc.loop();
```

The controller sends the introduction before initial feedback. Mode 1 remains
the default; pass `APC40MKII::sysex::MODE_2` as the controller's second constructor
argument to select the fully host-controlled LED mode for hardware testing.
Repeated `init()` calls do not duplicate controls or reset bank selection.
Use the focused test command below; bundled RtMidi tests have separate MIDI
hardware and runtime-DLL requirements.

## Hardware-Free Tests

These tests link the same `apc40_core` library as the host and inject a fake
`APC40Transport`. No real or fake RtMidi header, MIDI driver, MIDI device, or
RtMidi build is needed. Use a separate directory so this does not change the
host configuration.

```sh
cmake -S . -B build-tests -DAPC40_BUILD_HOST=OFF -DBUILD_TESTING=ON
cmake --build build-tests --config Debug --target apc40_tests
cmake -E chdir build-tests ctest -C Debug -R apc40_native --output-on-failure
```

The default 15-case suite covers bank selection, releases, invalid bank indices,
independent bank values, recalled ring feedback, MIDI channel filtering,
outgoing MIDI bytes, and application-driven updates without callback echo. It
also covers malformed MIDI, note-on-zero releases, bounded seven-bit SysEx
encoding, introduction ordering, simulated reconnection, signed relative
encoder boundaries, fader routing, and the logical API for all 24 track-knob
slots. CC consumption tests cover bound/unbound callbacks, wrong channels,
registered controls, raw observers, and bank storage. Stable names and invalid
name lookups are checked with the optional parameter adapter disabled, without
Arduino or parameters headers.

Transport-button cases check pre-init/offline state, press versus release,
held-note suppression, wrong-channel input, software-driven LEDs, legacy callback
fallback, reconnect without actions, and retry after a failed LED send.

Enable `APC40_ENABLE_MIDI_LEARN` in the CMake cache and rebuild `apc40_tests`
to include the sixteenth case. The public compile definition applies
`ENABLE_PARAMETERS_MIDI_LEARN` consistently to the core and its callers.
This case needs no parameter headers: it checks logical candidates, unchanged
CC consumption, rejected and accepted capture, input-time bank identity even
when callbacks switch banks, SESSION blinking/retry, cancellation, and reconnect.

## Transport Buttons

PLAY, RECORD, and SESSION expose `onPlayPressed`, `onRecordPressed`, and
`onSessionPressed` callbacks. A callback is invoked once per press, not on
release or repeated Note On while held. Nonzero Note Off and Note On with zero
velocity both release the button. Only MIDI channel 1 is accepted.

Presses do not replace desired LED state with the incoming velocity or toggle
application state automatically. Applications supply that state through
`setPlayState(bool)`, `setRecordState(bool)`, and `setSessionState(bool)`:

```cpp
apc.setPlayState(app.isPlaying());
apc.onPlayPressed = [&] {
  app.togglePlayback();
  apc.setPlayState(app.isPlaying());
};
```

These setters also work before `init()` and while disconnected. Identical state
is not resent, and failed sends remain pending for `apc.loop()` to retry.
Reconnect clears held-button and sent-feedback state, then restores desired
LEDs after the introduction without invoking press callbacks. Call the setters
when software state changes, or sample application state from the normal loop.
The existing `setAppValue()`/`redraw()` path still works; `apc.loop()` also checks
these three controls for app-value changes.

For compatibility, an existing transport control's `onAPCValueChanged` is used
as a press-only fallback if its explicit `on...Pressed` callback is unset. When
both are installed, only the explicit callback runs. `sendNoteOn()` and
`sendNoteOff()` now return the backend send result; callers may ignore it.

Nexus6 uses `clock_stop()`/`clock_continue()` for pause/resume, without resetting
song position, and mirrors its authoritative `playing` state. RECORD toggles
the app-owned `transport_state.record_enabled`, initially false. This is a mode
flag, not an implemented recorder. Other software can change that flag directly
or use the behavior's `set_record_enabled()`; the regular behavior loop updates
the LED even while playback is paused.

With learning disabled, SESSION is inactive by default. Nexus6 exposes
`on_session_pressed` and `set_session_state(bool)` for application use.

## MIDI Learn

Define `ENABLE_PARAMETERS_MIDI_LEARN` for all firmware translation units to
enable portable capture state and `onLearnCandidate(const APC40ControlEvent&)`.
The core never includes or calls `ParameterManager`. A candidate contains a
typed control kind, zero-based bank/index, input phase, and raw MIDI value.
Track and device knobs preserve the bank at input time; track/master/crossfaders
and nonzero Cue detents also produce candidates. Tempo, bank/transport buttons,
wrong-channel messages, and unknown controls do not. Button/pad kinds and
press/release phases reserve the extension point; they are not learnable yet.

The host must mirror its pending target through `setMIDILearnHostState(bool)`.
SESSION then arms/disarms device capture. An explicit `onSessionPressed`
callback remains authoritative and must forward that operation when desired.
SESSION is solid while the host is pending and blinks every 250 ms when both
sides are active. Supply `setMillisProvider()` for blinking; without a clock,
active capture stays solid. LED sends retain normal deduplication and retry.
Cancellation and disconnect clear device arming. Inactive feedback restores the
normal state set through `setSessionState()`.

Return `true` from `onLearnCandidate` only when an assignment succeeds; this
disarms device capture. Returning `false` leaves it armed. This result is
independent of CC consumption, so ordinary source updates still run and unbound
faders still reach a raw CC fallback. Bank recall and app-originated updates
never propose candidates.

With `APC40_ENABLE_PARAMETERS=1`, `APC40ParameterInputMap` provides checked,
constant-time `bind(controlId, input)` and `resolve(controlId)` operations.
It borrows input pointers, performs no registration/allocation during reception,
and returns `nullptr` for unbound or unsupported identities. It has slots for
24 track knobs, 64 device knobs, eight track faders, master, crossfader, and Cue.
The existing logical knob adapter also accepts an explicit `APC40ControlId`
for a device-bank source; `getKnobState`/`setKnobState` address either knob kind.

Nexus6 maps its existing 35 inputs, without allocating new device-bank inputs.
Unregistered device knobs therefore leave capture armed. The behavior owns
mapping and manager calls; manager target revisions require a fresh SESSION
press after a target switch, cancellation, or completion, even before the next
regular loop. Learning assigns one modulation-slot source through
`set_slot_input`, preserves amount/polarity, and clears the pending target and
its runtime indicator. Selecting the same menu learn indicator cancels it.
This is source learning, not a direct parameter-base-value binding engine.

## Banked Controls

After `APC40Controller::init()`, `bank_sets[0]` is the track-knob bank set
(Pan, Sends, User), and `bank_sets[1]` is the device-knob bank set.

- `onBankedChange(encoderIndex, bank, value)` delivers hardware changes to the
  application. Bind it to your sequencer or parameter setter.
- `setEncoderValue(encoderIndex, bank, value)` delivers application changes to
  the bank. An active-bank update sends ring feedback immediately; an inactive
  update is stored until that bank is selected. It does not call `onBankedChange`.
- `setBank(bank)` changes selection and redraws all bank LEDs and encoder rings.

Bank and encoder indices are zero-based. Values are raw MIDI values, 0-127;
application updates above 127 are clamped. MIDI channels in the device API are
zero-based too: channel `0` means MIDI channel 1. The current bank sets use
channel `0`.

Each banked encoder has independent value, polarity, and ring-mode state. Use
`getEncoderState()` and `setEncoderState()` with a bank and encoder index to copy
the complete state in or out. The default `DeviceDefault` ring mode leaves the
APC's native strategy untouched; bank redraw sends the value without a ring-mode
CC. `Automatic` is an opt-in strategy that maps unipolar to Volume and bipolar
to Pan. `setEncoderRingMode()` can also choose Off, Single, Volume, or Pan
explicitly. For these configured modes, bank redraw sends the ring-mode CC
before the value CC. Physical turns update the retained source and call
`onBankedChange` without echoing a CC back to the APC; application value changes
and bank recalls still send value feedback.

```cpp
APC40BankSet* tracks = apc.bank_sets[0];
tracks->setEncoderValue(2, 1, 96);
tracks->setBank(1);
```

For custom bank definitions, use named arrays rather than C compound literals
such as `(uint8_t[]){...}`, which are not standard C++ and fail with MSVC.
The constructor accepts `const uint8_t[]` and `const char* const[]`. Bank names
are borrowed: the name table and strings must outlive the bank set. Static
arrays are suitable for fixed firmware layouts.

`APC40Controller` also exposes input callbacks for faders and relative
encoders:

- `onTrackFaderChanged(trackIndex, value)` uses zero-based track indices `0-7`
  and absolute MIDI values `0-127`.
- `onMasterFaderChanged(value)` and `onCrossfaderChanged(value)` report absolute
  values `0-127` on MIDI channel 1.
- `onTempoEncoderDelta(delta)` and `onCueEncoderDelta(delta)` report signed
  `int16_t` steps. Only Tempo CC13 and Cue CC47 use this decoder; `0x40` is `-64`.

Acceleration is disabled by default. Applications can provide a monotonic
millisecond clock and configure independent thresholds and multipliers for
Tempo and Cue; the controller applies the configured multiplier to detents
arriving within the threshold. For example, on Arduino:

```cpp
apc.setMillisProvider([] { return millis(); });
apc.setTempoEncoderAcceleration({60, 3});
```

The host example prints the resulting steps. BPM range, step scaling, and any
further application policy stay outside the controller.

For application code, the 24 track knobs are addressed logically with
`APC40TrackKnobBank::{Pan, Sends, User}` and a zero-based track index. Use
`getTrackKnobState` and `setTrackKnobState` to retain each slot's value,
polarity, and ring mode, and `onTrackKnobChanged(bank, track, value)` for
physical turns. This API does not require applications to know the APC's CC
numbers or bank-button notes.

Library-owned labels are available through `getTrackKnobBankName(bank)`,
`getTrackKnobGroupName(bank)`, `getTrackKnobName(bank, track)`, and
`getTrackFaderName(track)`. These static controller methods return borrowed,
static strings, or `nullptr` for invalid indices. They preserve the existing
Nexus6 labels such as `APC40 Pan`, `PanKnob1`, and `TrackFader1`.

## Transport Contract

Implement `APC40Transport` to use another backend. The controller borrows the
transport; construct the transport first and keep it alive until after the
controller is destroyed. One device exclusively owns the transport callbacks.

- `open()` is idempotent and returns whether a usable input/output pair exists.
- `isConnected()` reports the backend's current connection state.
- `close()` closes the connection and discards input from that connection.
- Report connection transitions through `onConnectionChanged(bool)` after
  updating the backend's connection state. A successful reconnect must report
  `true` even when the same physical device returns.
- Deliver complete MIDI messages through `onMessage(data, size)`, not individual
  USB packets or running-status fragments. The data need only remain valid
  until the callback returns. Its callback now returns `bool`; transport
  backends may ignore the result unless they need a fallback receive path.
- `sendMessage(data, size)` must consume or copy the bytes before returning.
  The core passes temporary stack buffers. Return `false` on failure.
- Deliver callbacks on the application thread, normally from `poll()` invoked
  by `apc.loop()`. Do not invoke application callbacks from a USB interrupt or
  background MIDI thread.

The RtMidi adapter defaults to matching the port-name substring `APC40`, and
dispatches at most 64 messages from its 256-message input queue per loop. It does
not implement reliable physical unplug detection or automatic reopen; an
explicit `close()`/`open()` reports the transitions. RtMidi's `isPortOpen()` can
remain true after unplugging, so it is not proof of physical connection.

Device input callbacks use `(number, channel, value)`; outgoing device helpers
use `(number, value, channel)`. Channels are zero-based. Note-on velocity zero
is normalized to `onNoteOff`. Short or invalid channel messages are ignored.
The core accepts complete SysEx through `onSystemExclusive`; it does not yet
interpret device identity or the introduction response.

For CC messages, `onMessage` returns `true` when the controller stores a banked
value, applies a matching registered encoder, or invokes a bound surface
callback. It returns `false` for unhandled or malformed messages. A recognized
fader or relative-encoder CC is not consumed without a bound callback; for
example, unbound master and crossfaders can still reach normal MIDI CC inputs.
This decision is made during normal processing, without a second lookup.
For notes and SysEx, `true` means the corresponding device callback was invoked;
it does not provide per-control note-consumption tracking.

The controller installs `APC40Device::handleControlChange` as the consuming
handler. `onControlChange` remains a void raw observer, called once after that
handler for each valid CC; observing a message does not consume it. Custom
transport `onMessage` callbacks must now return a boolean, while existing
void device observers need no changes. An application that bridges another
MIDI API can return the transport callback's result and use it for fallback:

```cpp
const bool handled = transport.receiveMidiMessage(0xB0, channel, number, value);
if (!handled) {
  MIDI_CC_Source::update_parameter_inputs_cc(number, value, channel);
}
```

Here `receiveMidiMessage` is an application transport helper, not a core method.
The Nexus6 USBHost helper also converts one-based USBHost channels to the core's
zero-based MIDI channels.

When a transport reports reconnection, the controller sends the introduction,
restores selected banks and their values, and redraws registered control state.
Offline bank updates are retained. This does not fire hardware-change callbacks.
Direct `APC40Feedback` calls are not cached or replayed yet. For a registered
button, use `setAppValue()` followed by `apc.redraw(control)` for immediate LED
feedback; arbitrary LEDs and RGB layers still need the planned state model.

## Arduino Boundary

This repository is also a PlatformIO Arduino library. Its `library.json`
excludes the RtMidi host, Windows port query, desktop example, and test entry
points; Nexus6 can include `APC40Controller.h` and link the library's core
sources. The controller's `GenericList` dependency is provided by the existing
local `foundling` library already declared in Nexus6's `platformio.ini`.

The core builds without RtMidi or desktop logging, but this is not yet a packaged
Arduino library. Arduino builds must exclude the desktop main, port tool, and
RtMidi transport; an Arduino USB-host backend and packaging remain to be added.
The core still depends on foundling's `GenericList` and allocates its controls
and banks during setup. Cleanup and explicit allocation ownership must be
completed before dynamic bank creation/removal is supported.

Nexus6, `parameters`, and storage are not dependencies of the portable core.

### Optional Parameter Inputs

Applications using the `parameters` library can opt into the header-only
`APC40ParameterInputs.h` adapter. Define `APC40_ENABLE_PARAMETERS` to `1` before
including that header (or via build flags):

```cpp
#define APC40_ENABLE_PARAMETERS 1
#include <APC40ParameterInputs.h>

// After apc.setup_banks() or apc.init():
APC40LogicalKnobParameterInput pan(apc, APC40TrackKnobBank::Pan, 0);
APC40CueKnobParameterInput cue((char *)"CueKnob", "APC40");

// In application startup:
// apc.onCueEncoderDelta = [&](int16_t delta) { cue.apply_delta(delta); };
```

Without that flag, the header exposes no adapter classes and includes neither
Arduino nor parameters headers. It is never included by the core. Enabling the
adapter requires Arduino and the consuming project's existing `parameters`
dependency; it does not register inputs or add a storage dependency itself.

`APC40LogicalKnobParameterInput` reads/writes the controller's independent bank
state instead of claiming a raw MIDI CC identity. Its default constructor labels
come from the core; the original explicit `(name, group, controller, bank, track)`
constructor is also supported. Values clamp to 0-127. It supports unipolar and
bipolar normalization and uses automatic ring modes; run its `loop()` when
changing `input_type` to synchronize polarity without redundant feedback.

`APC40CueKnobParameterInput` retains a clamped accumulated value updated by
`apply_delta()`. It deliberately ignores direct raw CC delivery so relative
events cannot overwrite its state. Input registration, callback binding, BPM
policy, and persistence remain application responsibilities. Nexus6's existing
native adapter test checks these classes with mocked USB hardware and the real
core, including CC fallback and retained ring/value state after reconnect.
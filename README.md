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

The 13-case suite covers bank selection, releases, invalid bank indices,
independent bank values, recalled ring feedback, MIDI channel filtering,
outgoing MIDI bytes, and application-driven updates without callback echo. It
also covers malformed MIDI, note-on-zero releases, bounded seven-bit SysEx
encoding, introduction ordering, simulated reconnection, signed relative
encoder boundaries, fader routing, and the logical API for all 24 track-knob
slots.

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
  until the callback returns.
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

Parameter range conversion belongs in the optional adapter, not the transport.
Nexus6, `parameters`, and storage are not dependencies of the core and were not
changed by this first implementation step.
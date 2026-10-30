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
cmake --build build --config Debug --target apc40_host
```

On Windows, run `build/Debug/apc40_host.exe`. The current device discovery looks
for MIDI ports whose names contain `USB Audio Device`. Pass another name
substring to the transport constructor when necessary. Windows host builds copy
the RtMidi DLL beside `apc40_host` and `list_ports` when building a shared RtMidi.

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

The suite covers bank selection, releases, invalid bank indices, independent
bank values, recalled ring feedback, MIDI channel filtering, outgoing MIDI
bytes, and application-driven updates without callback echo. Nine cases also
cover malformed MIDI, note-on-zero releases, bounded seven-bit SysEx encoding,
introduction ordering, repeated initialization, and simulated reconnection.

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

The RtMidi adapter uses its 256-message input queue and dispatches at most 64
messages per loop. It retains the original port-name matching behavior. It does
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

The core builds without RtMidi or desktop logging, but this is not yet a packaged
Arduino library. Arduino builds must exclude the desktop main, port tool, and
RtMidi transport; an Arduino USB-host backend and packaging remain to be added.
The core still depends on foundling's `GenericList` and allocates its controls
and banks during setup. Cleanup and explicit allocation ownership must be
completed before dynamic bank creation/removal is supported.

Parameter range conversion belongs in the optional adapter, not the transport.
Nexus6, `parameters`, and storage are not dependencies of the core and were not
changed by this first implementation step.
# APC40 MkII Integration Plan

## Agreed Direction

- Keep hardware protocol and bank/control state APC40 MkII-specific for now.
  Separate reusable transport and binding concepts without building a universal
  controller framework prematurely.
- Pan, Sends, and User select three banks of eight independent track-knob
  sources: 24 sources, not eight sources whose values are replaced on selection.
  Inactive sources retain their values and continue to supply modulation.
- Each virtual knob retains its own polarity. Ring style is independently
  configurable, with Volume and Pan as unipolar/bipolar defaults.
- The application owns playback, tempo policy, assignments, focus, dashboard
  layouts, clip semantics, and scene/project persistence scope.
- No focused-device or dashboard UX currently exists. Explicit assignments come
  first; focus and pinned assignments must not block initial integration.
- An optional `parameters` adapter may add reusable manual/writable inputs or
  bindings. Neither `parameters` nor storage becomes a core dependency.
- Give a control one active role unless the application explicitly opts into
  multiple actions. Parameter editing initially reflects base values rather
  than continuously following modulation output.

## First Milestone: Implemented

- `apc40_core` is a reusable CMake target with no RtMidi or desktop logging.
- `APC40Device` borrows an injected `APC40Transport`; the native host uses a
  separate RtMidi implementation with application-loop input delivery.
- Existing banking and feedback behavior passes against a fake transport.
- Channel-message parsing rejects malformed input and normalizes note-on-zero
  to release. SysEx output is bounded and uses seven-bit length fields.
- Initialization sends the introduction before feedback. Repeated initialization
  preserves controls and banks. Reported reconnects restore known bank/control
  state without replaying hardware actions.
- Mode 1 remains the default. Mode 2 is selectable for fully host-controlled LED
  testing; a default change awaits hardware validation.
- Nine native regression cases pass under GCC and MSVC. The real Windows host
  and port tool build; host DLL deployment is independent of working directory.

This is a portable-core foundation, not completed Arduino integration or full
reconnect support. Arbitrary feedback is not cached, physical hotplug detection
is backend-specific and unfinished, and allocation cleanup remains to be done.

## Next: Explicit Virtual Knob State

1. Make each bank slot's value readable and writable through a small public API.
   Define stable slot identity from bank set, bank, and encoder, not pointers.
2. Add per-slot polarity and ring style. Recall both ring configuration and
   value, sending ring mode before value on selection and reconnect.
3. Keep hardware-change callbacks distinct from application feedback. Switching
   banks must never change inactive source values or emit source changes.
4. Define who owns allocated controls, banks, lists, and bindings; provide
   cleanup and prevent unsafe copying. Allocate configuration at setup, not
   while handling input. Do not promise dynamic bank removal yet.

Acceptance: populate all 24 track slots with distinct values and polarities;
switch every bank and simulate reconnect; every slot remains independent and
only physical edits invoke the source-change callback.

## Then: Events And Desired Feedback

1. Add explicit control capabilities and channel rules for the remaining
   buttons, faders, encoders, and grid. Avoid broad note-range heuristics.
2. Expose press/release events independently from desired LED state. Handle
   note-off regardless of its velocity and clear transient held/Shift state on
   disconnect without changing authoritative application state.
3. Decode only Tempo CC13 and Cue CC47 as signed seven-bit two's complement.
   Cover values 0, 1, 63, 64, 65, and 127; value 64 means -64, not +64.
4. Add desired versus sent feedback state, ring mode/value, mono LED states,
   and RGB palette/effect layers. RGB output channels select animation effects,
   not tracks. The application decides whether transient animations replay.
5. Invalidate sent state on reconnect and redraw after introduction. Bound
   feedback work per loop and retain dirty state when sending fails.

Acceptance: changing Play in the app updates its LED without input echo;
all desired persistent feedback returns after reconnect, with no replayed
transport actions. Verify RGB palette, effects, and timing on real hardware.

## Then: Minimal Nexus6 Integration

1. Implement a USB-host transport with explicit attach/detach notification and
   queued application-thread delivery. Use the existing APC USB identity wiring
   as a starting point, not the old behavior stub as an API contract.
2. Package or clearly select core-only Arduino sources so desktop entry points
   and RtMidi cannot enter the firmware build.
3. Register 24 track-knob sources and eight track faders once, not on reconnect.
   Add Play state/action and signed tempo changes with app-defined scaling.
4. Add a small optional `parameters` adapter. Normalize source values and map
   polarity there; apply edits to base parameter values through explicit
   application assignments. Do not globally enable unrelated storage features.
5. Extend device-knob/button assignments after the basic source path works.
   Decide jump versus pickup when non-motorized faders change roles.

Acceptance: user-run firmware build and hardware smoke test; all 24 sources
continue modulating across bank changes and USB reconnection, with registrations
and app state preserved. A native test is not a substitute for this check.

## Later: Assignments And Persistence

Add app-defined focus, pinned/dashboard assignments, remaining controls, and
optional clip-grid behavior as their application models become concrete. Save
assignments with stable identifiers and explicit versioning. The application
chooses scene versus project scope; the core must not choose storage policy.

## Protocol And Hardware Checks

Use the user-supplied Akai APC40 mkII Communication Protocol v1.2, dated
2015-01-19. Its introduction-response length and some generalized control tables
are inconsistent. Prefer explicit control entries and captured messages; do not
encode contradictory table text as assumptions.

Hardware checks still needed: actual MIDI port names and identity, Mode 1 versus
Mode 2 behavior, ring modes and polarity, relative encoder signs, RGB animation
timing, incoming introduction response, and unplug/replug behavior on each
backend. No additional information is blocking the next core milestone.
# Input and controller runtime specification

This document defines the Phase 2 platform-input and controller boundary. The
current project-authored `InputAccumulator` records held digital actions,
press/release edges, and four signed 16-bit axes in tick-addressed snapshots.
Focus loss can release held actions and neutralize axes. SDL translation and
device management now cover the graphics menu's digital controller buttons.
Gameplay translation, remapping, glyphs, rumble and recovered retail mappings
are not implemented yet.

## Implemented graphics-menu controller path

The native runtime opens the first available SDL gamepad and handles attachment
and removal by instance identity. It closes its controller before shutting down
the gamepad subsystem. Start maps to F10; D-pad directions navigate and adjust;
south confirms/activates; east cancels or requests display rollback only while
the menu is visible. East with a closed menu cannot request application exit.
Controller and keyboard commands share the same transactional apply/confirm/
revert handler, and confirmation timeout remains independent of device state.

Button commands use press edges, not continuous polling or automatic repeat.
Held buttons are baselined when selecting a controller and regaining focus;
unfocused and stale queued commands cannot confirm settings. Removal itself
does not accept, cancel or revert the draft. Another available controller may
be selected, with a fresh baseline. This is an explicit portable menu policy,
not evidence of original gameplay controls. Physical Steam Deck and other
controller acceptance runs remain required; virtual SDL tests are not a
substitute for hardware validation.
The virtual-device test exercises SDL enumeration, button events, removal,
replacement, focus baselines and a display-confirmation rollback. It skips
explicitly when virtual attachment is unsupported or other gamepads are already
connected, to avoid depending on a developer's physical device order. On the
ARM64 Linux development host it ran successfully, not skipped. A supported
Steam-data launch with the F10 menu visible also completed through Vulkan after
this integration; that smoke test does not establish physical button behavior.

## Layering

Platform events pass through four explicit layers:

1. SDL identifies keyboards, mice, gamepads, Steam Deck controls, and connection
   changes. Platform keycodes never enter authoritative simulation.
2. A profile maps physical controls to named gameplay or UI actions. Multiple
   devices may contribute, with a stable device class and conflict policy.
3. Axis processing applies calibrated center, inner/outer dead zones, inversion,
   saturation, and response curves, then quantizes once to signed 16-bit values.
4. `InputAccumulator` samples actions at the next simulation tick. Rendering may
   inspect presentation input but cannot resample an authoritative snapshot.

Filtering uses deterministic declared arithmetic. Event timestamps, poll
frequency, refresh rate, and controller enumeration order cannot change a
recorded replay. Original and Modern consume identical gameplay snapshots.

## Action domains

The implemented vocabulary is currently limited to four movement directions,
fire, aim, interact, squad command, and move/look axes. These names are portable
infrastructure, not proof of the complete retail action set or original mapping.

The recovered Phase 2/4 catalog must separately define continuous and
edge-triggered gameplay actions, squad commands, aiming/camera axes, menu
navigation and adjustment, F10 plus an independently bindable controller menu
shortcut, and pause/system actions that are never serialized as gameplay.

UI repeat delay and rate belong to the UI clock and never create repeated
simulation presses. Simultaneous opposing directions resolve to a neutral axis
unless recovered Original behavior proves another rule.

## Devices, ownership, and hotplug

The vertical slice supports keyboard/mouse and one active SDL gamepad on Windows,
Linux, Steam Deck, and macOS. The most recent intentional gameplay input selects
the glyph family; stick drift cannot switch glyphs. Runtime device IDs are not
stored as stable identities.

Disconnecting an active device releases its controls at the next tick and cannot
leave fire, movement, or menu input latched. Reconnection does not restore held
state. Focus loss uses the same release path and pauses relative-mouse capture
until deliberately reacquired. Hotplug in a menu preserves draft settings but
resets navigation repeats.

Mappings prefer SDL's controller database and expose a generic fallback only if
all required controls can be identified. Steam Deck is a native SDL gamepad, not
a Proton or emulated XInput path. Rumble is presentation-only and cannot affect
simulation state.

## Configuration and accessibility

Bindings are action-based and stored by symbolic action plus device class.
Unknown actions/capabilities are ignored safely; schema versioning and defaults
recover from corrupt profiles. Essential gameplay and menu actions cannot be
left unreachable, conflicts are shown before commit, and reset-to-default is
transactional.

Required options include independent X/Y look inversion, mouse and stick
sensitivity, inner/outer dead zones, response curve, aim toggle/hold, rumble
enable/intensity, and glyph family. Original defaults require recorded retail
behavior. Accessibility alternatives cannot alter a replay after its quantized
snapshots are recorded.

## Acceptance evidence

Tests cover edge coalescing, held persistence, opposing input, quantization and
dead-zone boundaries, focus loss, disconnect/hotplug, binding conflicts, corrupt
profiles, and UI repeat. Equivalent scripted physical input must produce
byte-identical `InputSnapshot` streams across platforms.

The Phase 2 gate requires keyboard/mouse and controller navigation through
startup, F10, and the static-level path on every target, including
controller-only recovery from an invalid display draft. Phase 4 additionally
requires recovered Original action semantics and mappings validated against
private black-box traces, not inferred from current action names.

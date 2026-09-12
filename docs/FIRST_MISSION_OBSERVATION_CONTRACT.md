# First-mission observation contract

This contract is the next evidence gate before a gameplay subsystem can claim
retail compatibility. It deliberately records behavior, not retail content. It
does not authorize a mission implementation, a guessed player controller, or a
synthetic playable substitute.

## Why this gate comes first

The current `SimulationWorld` is useful deterministic infrastructure, but it
has no recovered component schema, player identity, world transform, collision
shape, mission transition, or gameplay-system ordering. The available private
observation establishes only that the first controllable frame is third-person,
interior, has HUD regions, and shows a movement tutorial. That is insufficient
to select locomotion constants, map an input action, create a player, or decide
when a mission starts. Implementing any of those now would invent behavior.

The largest open gameplay dependency is therefore a repeatable first-mission
state trace, beginning at the handoff from the opening sequence and ending at
one independently observable state transition. The trace is collected only
from a user-owned installation and remains private. It must not contain file
paths, addresses, screenshots, audio, dialogue, raw strings, object names,
serialized payloads, asset identifiers, or executable bytes.

## Required runs

Capture at least two clean launches of the same verified data build. Each run
must use the same declared profile, difficulty, display mode, and input device
class. A third run changes exactly one input experiment. Do not attach to,
modify, suspend, or patch an unrelated live game process.

The private observer records one categorical row at each externally visible
boundary:

| Boundary | Required categories |
| --- | --- |
| Opening handoff | cinematic-visible, loading-visible, gameplay-viewport-visible |
| Control | no-response, movement-only, camera-only, movement-and-camera, blocked-by-overlay |
| Camera | fixed, follows-translation, rotates-with-look, independently-rotatable, unknown |
| Player state | absent, spawned, controllable, disabled, unknown |
| Collision probe | no-contact, blocked, sliding, stepped, falling, unknown |
| Interaction probe | unavailable, prompt-only, entered-range, accepted, rejected, unknown |
| Mission state | unchanged, objective-advanced, failed, completed, loading, unknown |
| HUD state | absent, stable, changed, hidden, unknown |

The observer may additionally retain private frame numbers, monotonic elapsed
time, and input-event order. Public evidence may publish only non-identifying
counts, repeat agreement, declared method version, data-manifest fingerprint,
platform/architecture, and pass/fail results.

## Input experiment matrix

Starting from the first controllable state, perform short, isolated probes in
this order. Return to a fresh launch whenever a probe causes a state transition.

1. No input for a bounded observation interval.
2. One movement direction, then its opposite; repeat for each available
   movement control.
3. One look direction, then its opposite, without movement.
4. A movement-plus-look pair in both input orders.
5. Fire, aim, interact, squad command, pause/system action, and menu action
   individually, only where an input mapping is known from the platform.
6. A safe visible obstacle and a safe interaction candidate, if one is present,
   using one approach and one activation attempt.

For every accepted visible change, record its first and last boundary row and
whether repeating the identical probe agrees. Do not infer hidden health,
velocity, weapon, AI, collision, or mission values from pixels alone.

## Admission rule for implementation

A subsystem may move from `unscoped` to `specified` only when two independent
runs agree on its observable precondition, one action/input, one ordered visible
outcome, and one reset or terminal condition. The approved behavior-only
specification must name an explicit uncertainty for every unobserved branch.

No implementation may use the trace to hard-code a level path, retail text,
asset identity, object count, frame timing, or an inferred numeric constant.
The first bounded implementation should be the smallest behavior contract with
that four-part evidence, likely input-to-intent or a presentation-independent
handoff boundary. Locomotion, collision, weapons, AI, and mission scripting
remain blocked until their own observations satisfy the same rule.

## Public review bundle

Before an implementation change, a reviewer receives a source-free summary
with:

- observer and specification schema versions;
- opaque verified-data fingerprint and platform metadata;
- number of runs, probes, and agreeing boundary transitions;
- the subsystem being admitted, its stated inputs and outputs, and all retained
  unknowns; and
- proof that privacy/content scanning passed.

The complete private trace stays outside the repository. This bundle is enough
to assess whether a proposed bounded system follows measured behavior without
turning the repository into a redistribution channel.
